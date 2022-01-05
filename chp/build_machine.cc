#include <act/state_machine.h>
#include <act/passes/booleanize.h>
#include <act/iter.h>

namespace fpga {

static ActBooleanizePass *BOOL = NULL;

//Function to check whether variable is added to the list
//or not.
//0 - not declared
//1 - declared as variable
//2 - declared as port output
//3 - declared as instance port input
//4 - declared as instance port output
//5 - declared as port input
int is_declared (StateMachine *sm, act_connection *v) {

  std::vector<Port *> vp;
  vp = sm->GetPorts();
  for (auto iv : vp) {
    if (v == iv->GetCon()) {
      if (iv->GetDir() == 0) {
        return 2;
      } else {
        return 5;
      }
    }
  }

  std::vector<Variable *> vv;
  vv = sm->GetVars();
  for (auto iv : vv) {
    if (iv->GetCon() == v) {
      return 1;
    }
  }

  std::vector<StateMachineInst *> iv;
  iv = sm->GetInst();
  for (auto in : iv) {
    for (auto ip : in->GetPorts()) {
      if (v == ip->GetCon()) {
        if (ip->GetDir() == 1) {
          return 3;
        } else {
          return 4;
        }
      }
    }
  }
  return 0;
}

//Function to walk through expression tree and collect
//all variables for later analisys
void collect_vars(Expr *e, std::vector<ActId *> &vars) {
  if (e->type == E_VAR) {
    ActId *id = (ActId *)e->u.e.l;
    vars.push_back(id);
  } else if (e->type == E_INT ||
             e->type == E_REAL||
             e->type == E_BITFIELD) {
    return;
  } else if (e->type == E_FUNCTION) {
    fprintf(stdout, "sorry, no funcitons yet\n");
    return;
  } else {
    if (e->u.e.l) {collect_vars(e->u.e.l, vars); }
    if (e->type != E_COMPLEMENT) {
      if (e->u.e.r) {collect_vars(e->u.e.r, vars); }
    }
  }
}

//Function to traverse CHP description and collect all 
//neccessary data to to build state machine. Function 
//returns condition type to handle cases where return 
//state might be one of the previously added states.
//For exmaple, loops or commas.
Condition *traverse_chp(Process *proc, 
                        act_chp_lang_t *chp_lang, 
                        StateMachine *sm,   //current scope machine
                        StateMachine *tsm,  //top scope machine
                        Condition *pc,      //parent init cond
                        int is_sc = 0       //is parent a semi/comma
                        ) {
  Scope *scope;
  scope = proc->CurScope();
  act_boolean_netlist_t *bnl = BOOL->getBNL(proc);

  Condition *tmp;
  switch (chp_lang->type) {
  case ACT_CHP_COMMA: {

    //TODO: An interesting idea is to let lower machines
    //      know that they are a part of a COMMA statement
    //      which means they need an EXIT state, otherwise
    //      ASSIGN/SEND/RECV machines can be simplified by 
    //      removing EXIT state. Right now it always exists

    std::pair<State *, Condition *> n;
    std::vector<Condition *> vc;

    list_t *l;
    listitem_t *li;
    act_chp_lang_t *cl;

    //Comma type completion means concurrent completion of 
    //all comma'ed statements. If statement is simple then 
    //its evaluation happens at the current state of the 
    //current state machine. If Statement is complex, then 
    //new state machine is created.

    l = chp_lang->u.semi_comma.cmd;

    //Comma is a simple statement, so create new state
    //only if it is the top statement 
    State *s = NULL;
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_COMMA, sm->GetSize(), sm);
      sm->SetFirstState(s);
    }

    //if comma is top level statement then child condition
    //is for the current state otherwise use parent condition
    //to transfer to lower levels
    Condition *child_cond;
    if (s) {
      child_cond = new Condition(s, sm->GetSN(), sm);
    } else {
      child_cond = pc;
    }

    //traverse all COMMAed statements
    for (li = list_first(l); li; li = list_next(li)) {

      cl = (act_chp_lang_t *)list_value(li);

      //if skip statement simply ignore
      if (cl->type == ACT_CHP_SKIP || 
          cl->type == ACT_CHP_FUNC) {
        continue;
      }

      //if statement is COMMA then no new sm
      //is needed otherwise create new child sm
      if (cl->type == ACT_CHP_COMMA || cl->type == ACT_CHP_SEMI) {
        tmp = traverse_chp(proc, cl, sm, tsm, child_cond, 1);
      } else {
        StateMachine *csm = new StateMachine();
        csm->SetNumber(sm->GetKids());
        csm->SetParent(sm);
        csm->SetProcess(sm->GetProc());
        sm->AddKid(csm);
        tmp = traverse_chp(proc, cl, csm, tsm , child_cond, 1);
      }

      //if valid condition is returned then add it
      //to the termination vector
      if (tmp) {
        vc.push_back(tmp);
      }

    }

    //if comma is top level then need to add
    //child condition to the termination list
    if (s) {
      sm->AddCondition(child_cond);
      vc.push_back(child_cond);
    }

    //creating general termination conditon
    Comma *term_com = new Comma;
    term_com->type = 0;
    term_com->c = vc;
    Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);

    //if there is no parent sm then create dummy
    //SKIP state and after termination switch to it
    if (s) {
      State *exit_s = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
      //sm->AddSize();
      n.first = exit_s;
      n.second = term_cond;
      s->AddNextState(n);
    }
  
    sm->AddCondition(term_cond);

    return term_cond;

    break;
  }
  case ACT_CHP_SEMI: {

    std::pair<State *, Condition *> n;

    list_t *l;
    listitem_t *li;
    act_chp_lang_t *cl;

    l = chp_lang->u.semi_comma.cmd;

    Condition *child_cond = NULL;

    //Semi is a simple statement, so create new state
    //only if it is the top statement
    State *s = NULL;
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_SEMI, sm->GetSize(), sm);
      sm->SetFirstState(s);
    }

    Comma *first_com = new Comma();
    first_com->type = 0;
    Condition *first_cond = NULL;

    //traverse all statements separated with semicolon
    for (li = list_first(l); li; li = list_next(li)) {

      cl = (act_chp_lang_t *)list_value(li);

      //if statement is SKIP then simply ignore it
      if (cl->type != ACT_CHP_SKIP && cl->type != ACT_CHP_FUNC) {

  //      if (li == list_first(l)) {
  //        if (s) {
  //          child_cond = new Condition (s, sm->GetSN(), sm);
  //          sm->AddCondition(child_cond);
  //        } else {
  //          child_cond = pc;
  //        }
  //      } else {
  //        if (pc) {
  //          Comma *child_com = new Comma();
  //          child_com->type = 0;
  //          child_com->c.push_back(pc);
  //          child_com->c.push_back(tmp);
  //          child_cond = new Condition(child_com, sm->GetCCN(), sm);
  //          sm->AddCondition(child_cond);
  //        } else {
  //          child_cond = tmp;
  //        }
  //      }
 
        if (li == list_first(l)) {
          if (s) {
            child_cond = new Condition (s, sm->GetSN(), sm);
            sm->AddCondition(child_cond);
          } else {
            //Create first_com and first_cond to loop SEMI
            //execution
            first_com->c.push_back(pc);
            first_cond = new Condition(first_com, sm->GetCCN(), sm);
            child_cond = first_cond;
            sm->AddCondition(first_cond);
          }
        } else {
          if (pc) {
            Comma *child_com = new Comma();
            child_com->type = 0;
            child_com->c.push_back(pc);
            child_com->c.push_back(tmp);
            child_cond = new Condition(child_com, sm->GetCCN(), sm);
            sm->AddCondition(child_cond);
          } else {
            child_cond = tmp;
          }
        }

        if (cl->type == ACT_CHP_COMMA || cl->type == ACT_CHP_SEMI) {
          tmp = traverse_chp(proc, cl, sm, tsm, child_cond, 1);
          if (s) {
            sm->AddCondition(tmp);
          }
        } else {
          StateMachine *csm = new StateMachine();
          csm->SetNumber(sm->GetKids());
          csm->SetParent(sm);
          csm->SetProcess(sm->GetProc());
          sm->AddKid(csm);
          tmp = traverse_chp(proc, cl, csm, tsm, child_cond, 1);
        }
      }
    }

    //This condition is created together with
    //the first_cond to loop SEMI execution, i.e.
    //when the last statement completes execution
    //first statement should return to the initial
    //state before waiting for the parent machine
    //switching its state. It is important in case
    //of the infinite loop  or one branch loops 
    //when parent state does not change
    Comma *neg_com = new Comma();
    neg_com->type = 2;
    neg_com->c.push_back(tmp); 
    Condition *neg_cond = new Condition(neg_com, sm->GetCCN(), sm);
    first_com->c.push_back(neg_cond);
    sm->AddCondition(neg_cond);
    Comma *term_com = new Comma();
    term_com->type = 0;
    term_com->c.push_back(tmp);
    Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
    if (s) {
      State *exit_s = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
      sm->AddSize();
      n.first = exit_s;
      n.second = tmp;
      s->AddNextState(n);
    }
    sm->AddCondition(term_cond);

    return term_cond;
    
    break;
  }
  case ACT_CHP_SELECT: {

    std::pair<State *, Condition *> n;
    std::vector<Condition *> vc;
    std::vector<Condition *> ve;
    //Selection statement completion happens after completion of
    //execution of one if the selction options. Thus in case of
    //return state existance function returns ORed comma condition
    //with options completion conditions.

    //Create initial state (guard will be evaluated here)
    //and corresponding state condition
    State *s = NULL;
    s = new State(ACT_CHP_SELECT, 0, sm);
    sm->SetFirstState(s);
    Condition *zero_s_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_s_cond);

    Condition *child_cond = NULL;
    Condition *guard = NULL;
    State *ss = NULL;

    //Flag for cases when parametarized select turns out
    //to be useless
    //[ pbool(false) -> bla bla
    //[]else -> skip
    //] -> nothing going to happen
    int empty_select = 0;
    int else_flag = 0;

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {

      if (gg->g){
        if (gg->g->type == E_FALSE) { 
          empty_select = 1;
          continue;
        }
        if (gg->s && gg->s->type != ACT_CHP_SKIP &&
                     gg->s->type != ACT_CHP_FUNC) {
          empty_select = 0;
          break;
        } else {
          empty_select = 1;
          continue;
        }
      } else {
        if (gg->s && gg->s->type != ACT_CHP_SKIP &&
                     gg->s->type != ACT_CHP_FUNC) {
          empty_select = 0;
          break;
        } else {
          empty_select = 1;
          continue;
        }
      }
    } 

    //Process all selection options
    if (empty_select == 0) {
      for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {
      
        if (gg->g) {
          if (gg->s && gg->s->type != ACT_CHP_SKIP
                    && gg->s->type != ACT_CHP_FUNC) {
            guard = new Condition(gg->g, sm->GetGN(), sm);
            sm->AddCondition(guard);
            ve.push_back(guard);
          } else {
            continue;
          }
        } else {
          if (gg->s->type == ACT_CHP_FUNC) {
            char tmp[1024] = "log";
            if (strcmp(tmp, gg->s->u.func.name->s) != 0){
              fatal_error("I don't know this function");
            }
          }
          guard = NULL;
          else_flag = 1;
        }

        if (gg->s) {
          if (gg->s->type != ACT_CHP_FUNC) {
            ss = new State(gg->s->type, sm->GetSize(), sm);
          } else {
            ss = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
          }
          sm->AddSize();

          Comma *guard_com = new Comma;
          guard_com->type = 0;
          guard_com->c.push_back(zero_s_cond);
          if (pc) { guard_com->c.push_back(pc); }

          if (else_flag == 1) {
            Comma *else_com = new Comma;
            else_com->type = 2;
            else_com->c = ve;
            Condition *else_guard;
            else_guard = new Condition(else_com, sm->GetCCN(), sm);
            sm->AddCondition(else_guard);
            guard_com->c.push_back(else_guard);
          } else {
            guard_com->c.push_back(guard);
          }

          Condition *full_guard;
          full_guard = new Condition(guard_com, sm->GetCCN(), sm);
          n.first = ss;
          n.second = full_guard;
          s->AddNextState(n);
          sm->AddCondition(full_guard);

          Condition *tmp_cond = NULL;
          if (pc) {
            Comma *child_com = new Comma;
            child_com->type = 0;
            child_com->c.push_back(pc);
            tmp_cond = new Condition(ss, sm->GetSN(), sm);
            sm->AddCondition(tmp_cond);
            child_com->c.push_back(tmp_cond);
            child_cond = new Condition(child_com, sm->GetCCN(), sm);
          } else {
            child_cond = new Condition(s, sm->GetSN(), sm);
          }
          sm->AddCondition(child_cond);

          if (gg->s->type == ACT_CHP_COMMA || gg->s->type == ACT_CHP_SEMI) {
            tmp = traverse_chp(proc, gg->s, sm, tsm, child_cond);
          } else if (gg->s->type == ACT_CHP_SKIP ||
                     gg->s->type == ACT_CHP_FUNC ){
            if (tmp_cond) {
              tmp = tmp_cond;
            } else {
              tmp = child_cond;
            }
          } else {
            StateMachine *csm = new StateMachine();
            csm->SetNumber(sm->GetKids());
            csm->SetParent(sm);
            csm->SetProcess(sm->GetProc());
            sm->AddKid(csm);
            tmp = traverse_chp(proc, gg->s, csm, tsm, child_cond);
          }
          if (tmp) {
            vc.push_back(tmp);
          } else {
            vc.push_back(child_cond);
          }
        }
      }

      //Create new condition to switch to the exit state
      //after execution completion
      Comma *exit_com = new Comma;
      exit_com->type = 1;
      exit_com->c = vc;
      Condition *exit_cond = new Condition(exit_com, sm->GetCCN(), sm);
      sm->AddCondition(exit_cond);
      
      //Create exit state
      State *exit_s = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
      sm->AddSize();
      
      n.first = exit_s;
      n.second = exit_cond;
      ss->AddNextState(n);
      
      Condition *exit_s_cond = new Condition(exit_s, sm->GetSN(),sm);
      sm->AddCondition(exit_s_cond);
      vc.push_back(exit_s_cond);

      //Create termination condition which is either
      //statement completion or exit state
      Comma *term_com = new Comma;
      term_com->type = 1;
      term_com->c = vc;
      Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
      sm->AddCondition(term_cond);

      //Return to the initial state when parent is not in 
      //the right state
      if (pc) {
        Comma *npar_com = new Comma;
        npar_com->type = 2;
        npar_com->c.push_back(pc);
        Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
        sm->AddCondition(npar_cond);
        n.first = s;
        n.second = npar_cond;
        exit_s->AddNextState(n);
      }

      return term_cond;

    } else {

      vc.push_back(zero_s_cond);
      if (pc) {
        vc.push_back(pc);
      }
      Comma *term_com = new Comma;
      term_com->type = 0;
      term_com->c = vc;
      Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
      sm->AddCondition(term_cond);

      return term_cond;

    }

    break;
  }
  case ACT_CHP_SELECT_NONDET: {

    std::pair<State *, Condition *> n;
    std::vector<Condition *> vc;
    std::vector<Condition *> ve;
    //Selection statement completion happens after completion of
    //execution of one if the selction options. Thus in case of
    //return state existance function returns ORed comma condition
    //with options completion conditions.

    //Create initial state (guard will be evaluated here)
    //and corresponding state condition
    State *s = NULL;
    s = new State(ACT_CHP_SELECT, 0, sm);
    sm->SetFirstState(s);
    Condition *zero_s_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_s_cond);

    Condition *child_cond = NULL;
    Condition *guard = NULL;
    State *ss = NULL;

    //Flag for cases when parametarized select turns out
    //to be useless
    //[ pbool(false) -> bla bla
    //[]else -> skip
    //] -> nothing going to happen
    int empty_select = 0;
    int else_flag = 0;

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {

      if (gg->g) {
        if (gg->s && gg->s->type != ACT_CHP_SKIP &&
                     gg->s->type != ACT_CHP_FUNC) {
          empty_select = 0;
          break;
        } else {
          empty_select = 1;
          continue;
        }
      } else {
        empty_select = 1;
        continue;
      }
    } 

    //Process all selection options
    if (empty_select == 0) {

      Arbiter *arb = new Arbiter();

      for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {
      
        if (gg->g) {
          if (gg->s && gg->s->type != ACT_CHP_SKIP
                    && gg->s->type != ACT_CHP_FUNC) {
            guard = new Condition(gg->g, sm->GetGN(), sm);
            Condition *arb_guard = new Condition(gg->g, sm->GetGN(), sm, 1);
            sm->AddCondition(arb_guard);
            arb->AddElement(arb_guard);
            ve.push_back(guard);
          } else {
            continue;
          }
        } else {
          if (gg->s && gg->s->type != ACT_CHP_SKIP
                    && gg->s->type != ACT_CHP_FUNC) {
            guard = NULL;
            else_flag = 1;
          } else {
            continue;
          }
        }
      
        if (gg->s) {
          ss = new State(gg->s->type, sm->GetSize(), sm);
          sm->AddSize();
      
          Comma *guard_com = new Comma;
          guard_com->type = 0;
          guard_com->c.push_back(zero_s_cond);
          if (pc) { guard_com->c.push_back(pc); }

          if (else_flag == 1) {
            Comma *else_com = new Comma;
            else_com->type = 2;
            else_com->c = ve;
            Condition *else_guard;
            else_guard = new Condition(else_com, sm->GetCCN(), sm);
            sm->AddCondition(else_guard);
            guard_com->c.push_back(else_guard);
            else_flag = 0;
          } else {
            guard_com->c.push_back(guard);
          }
          Condition *full_guard;
          full_guard = new Condition(guard_com, sm->GetCCN(), sm);
          n.first = ss;
          n.second = full_guard;
          s->AddNextState(n);
          sm->AddCondition(full_guard);
      
          if (pc) {
            Comma *child_com = new Comma;
            child_com->type = 0;
            child_com->c.push_back(pc);
            Condition *tmp_cond = new Condition(ss, sm->GetSN(), sm);
            sm->AddCondition(tmp_cond);
            child_com->c.push_back(tmp_cond);
            child_cond = new Condition(child_com, sm->GetCCN(), sm);
          } else {
            child_cond = new Condition(s, sm->GetSN(), sm);
          }
          sm->AddCondition(child_cond);
      
          if (gg->s->type == ACT_CHP_COMMA || gg->s->type == ACT_CHP_SEMI) {
            tmp = traverse_chp(proc, gg->s, sm, tsm, child_cond);
          } else {
            StateMachine *csm = new StateMachine();
            csm->SetNumber(sm->GetKids());
            csm->SetParent(sm);
            csm->SetProcess(sm->GetProc());
            sm->AddKid(csm);
            tmp = traverse_chp(proc, gg->s, csm, tsm, child_cond);
          }
          if (tmp) {
            vc.push_back(tmp);
          } else {
            vc.push_back(child_cond);
          }
        }
      }

      //Create new condition to switch to the exit state
      //after execution completion
      Comma *exit_com = new Comma;
      exit_com->type = 1;
      exit_com->c = vc;
      Condition *exit_cond = new Condition(exit_com, sm->GetCCN(), sm);
      sm->AddCondition(exit_cond);
      
      //Create exit state
      State *exit_s = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
      sm->AddSize();
      
      n.first = exit_s;
      n.second = exit_cond;
      ss->AddNextState(n);
      
      Condition *exit_s_cond = new Condition(exit_s, sm->GetSN(),sm);
      sm->AddCondition(exit_s_cond);
      vc.push_back(exit_s_cond);
      
      //Create termination condition which is either
      //statement completion or exit state
      Comma *term_com = new Comma;
      term_com->type = 1;
      term_com->c = vc;
      Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
      sm->AddCondition(term_cond);
      
      //Return to the initial state when parent is not in 
      //the right state
      if (pc) {
        Comma *npar_com = new Comma;
        npar_com->type = 2;
        npar_com->c.push_back(pc);
        Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
        sm->AddCondition(npar_cond);
        n.first = s;
        n.second = npar_cond;
        exit_s->AddNextState(n);
      }

      sm->AddArb(arb);

      return term_cond;

    } else {

      vc.push_back(zero_s_cond);
      Comma *term_com = new Comma;
      term_com->type = 0;
      term_com->c = vc;
      Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
      sm->AddCondition(term_cond);

      return term_cond;

    }

    break;
  }
  case ACT_CHP_LOOP: {

    //TODO: If a loop is infinite then there is no need for 
    //      a state machine we can loop internal state.
    //      This may be considered as BUFFER optimization
    //TODO: I can check guards in the end of each statement
    //      and switch to the EXIT state right after completion
    //      avoiding extra cycle on going to the IDLE state
    //      and checking guards there. MAYBE NEVER RETURN TO THE
    //      IDLE state after executing one of the branches

    std::pair<State *, Condition *> n;

    std::vector<Condition *> guards;
    std::vector<State *> states;
    std::vector<Condition *> state_conds;
    std::vector<Condition *> child_conds;
    std::vector<Condition *> terms;
    int br_idx = 0;

    //Loop type states keep executing while at least one guard
    //stays true. Termination condition is AND of all guards
    //with negation.

    int inf_flag = 0;

    //Create initial state and corresponding state condition
    State *s = NULL;
    s = new State(ACT_CHP_LOOP, 0, sm);
    sm->SetFirstState(s);
    Condition *zero_s_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_s_cond);

    Condition *child_cond = NULL;
      
    Condition *g = NULL;
    State *ss = NULL;

    //Collect all guard conditions
    //Collect all states and corresponding state conditions
    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {
      if (gg->g) {
        if (gg->s->type != ACT_CHP_SKIP && 
            gg->s->type != ACT_CHP_FUNC) {
          g = new Condition(gg->g, sm->GetGN(), sm);
          sm->AddCondition(g);
        } else {
          continue;
        }
      } else {
        inf_flag = 1;
        g = zero_s_cond;
      }
      ss = new State(gg->s->type, sm->GetSize(), sm);
      sm->AddSize();
      Condition *state_cond = new Condition (ss, sm->GetSN(), sm);
      sm->AddCondition(state_cond);
      guards.push_back(g);
      states.push_back(ss);
      state_conds.push_back(state_cond);
    }

    //Traverse CHP below to find termination conditions
    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {
      if (gg->g) {
        if (gg->s->type == ACT_CHP_SKIP || 
            gg->s->type == ACT_CHP_FUNC) { continue; }
      }

      //Creating init condition for the child sm
      //which is a corresponding branch state
      if (pc) {
        Comma *child_com = new Comma;
        child_com->type = 0;
        child_com->c.push_back(pc);
        child_com->c.push_back(state_conds[br_idx]);
        child_cond = new Condition(child_com, sm->GetCCN(), sm);
        sm->AddCondition(child_cond);
      } else {
        child_cond = state_conds[br_idx];
      }
      child_conds.push_back(child_cond);

      //Traverse lower levels of CHP hierarchy
      if (gg->s->type == ACT_CHP_COMMA || gg->s->type == ACT_CHP_SEMI) {
        tmp = traverse_chp(proc, gg->s, sm, tsm, child_cond);
      } else {
        StateMachine *csm = new StateMachine();
        csm->SetNumber(sm->GetKids());
        csm->SetParent(sm);
        csm->SetProcess(sm->GetProc());
        sm->AddKid(csm);
        tmp = traverse_chp(proc, gg->s, csm, tsm, child_cond);
      }
      //Collect all child termination conditions
      if (tmp) {
        terms.push_back(tmp);
      } else {
        terms.push_back(child_cond);
      }
      //Count the number of branches
      br_idx++;
    }

    //Here create conditions to iterate between branch
    //states instead of returning to the IDLE state for
    //guards reevaluation
    Comma *rti_com = new Comma; //ready to iterate
    rti_com->type = 1;
    Comma *sub_com;
    Condition *sub_cond;
    if (pc) {
      sub_com = new Comma;
      sub_com->type = 0;
      sub_com->c.push_back(pc);
      sub_com->c.push_back(zero_s_cond);
      sub_cond = new Condition(sub_com, sm->GetCCN(), sm);
      sm->AddCondition(sub_cond);
      rti_com->c.push_back(sub_cond);
    } else {
      sub_cond = zero_s_cond;
      rti_com->c.push_back(sub_cond);
    }
    for (auto i = 0; i < br_idx; i++) {
      sub_com = new Comma;
      sub_com->type = 0;
      sub_com->c.push_back(child_conds[i]);
      sub_com->c.push_back(terms[i]);
      sub_cond = new Condition(sub_com, sm->GetCCN(), sm);
      sm->AddCondition(sub_cond);
      rti_com->c.push_back(sub_cond);
    }
    Condition *rti_cond = new Condition(rti_com, sm->GetCCN(), sm);
    sm->AddCondition(rti_cond);
    for (auto i = 0; i < br_idx; i++) {
      Comma *it_com = new Comma;  //reiterate;
      it_com->type = 0;
      it_com->c.push_back(guards[i]);
      it_com->c.push_back(rti_cond);
      Condition *it_cond = new Condition(it_com, sm->GetCCN(), sm);
      sm->AddCondition(it_cond);
      n.first = states[i];
      n.second = it_cond;
      s->AddNextState(n);
    }

    //create condition for all negative guards
    Comma *nguard_com = new Comma;
    nguard_com->type = 2;
    nguard_com->c = guards;
    Condition *nguard_cond = new Condition(nguard_com, sm->GetCCN(), sm);
    sm->AddCondition(nguard_cond);

    //create general loop termination condition
    Comma *exit_com = new Comma();
    exit_com->c.push_back(rti_cond);
    exit_com->c.push_back(nguard_cond);
    exit_com->type = 0;
    Condition *exit_cond = new Condition(exit_com, sm->GetCCN(), sm);
    sm->AddCondition(exit_cond);

    //Create exit state to wait for parent to switch state
    State *exit_s = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
    sm->AddSize();
    n.first = exit_s;
    n.second = exit_cond;

    Condition *exit_s_cond = new Condition(exit_s, sm->GetSN(), sm);
    sm->AddCondition(exit_s_cond);

    s->AddNextState(n);

    if (pc || inf_flag == 1) {
      Comma *npar_com = new Comma;
      if (pc) {
        npar_com->type = 2;
        npar_com->c.push_back(pc);
      }
      else {
        npar_com->type = 0;
        npar_com->c.push_back(exit_cond);
        npar_com->c.push_back(tmp);
      }
      Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
      sm->AddCondition(npar_cond);
      n.first = s;
      n.second = npar_cond;
      exit_s->AddNextState(n);
    }

    Comma *term_com = new Comma;
    term_com->type = 1;
    term_com->c.push_back(exit_s_cond);
//TODO: This comment is not a good solution. Keep this in mind!!!
//    term_com->c.push_back(exit_cond);
    Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
    sm->AddCondition(term_cond);
    return term_cond;

    break;
  }
  case ACT_CHP_DOLOOP: {
    break;
  }
  case ACT_CHP_SKIP: {
    return NULL;
    break;
  }

  //Assignment is a state machine with two states.
  //First is IDLE and EXECUTION combined. It waits
  //till parent is in the right state to 
  //evaluate expression and switch to the EXIT state
  //at the next clock cycle.
  case ACT_CHP_ASSIGN: {

    std::pair<State *, Condition *> n;

    //Create initial state and corresponding condition
    State *s = NULL;
    s = new State(ACT_CHP_ASSIGN, 0, sm);
    sm->SetFirstState(s);
    Condition *zero_state_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_state_cond);

    //Create initial condition when both parent 
    //and child are in the right state
    Comma *init_com = new Comma();
    init_com->type = 0;
    init_com->c.push_back(zero_state_cond);
    if (pc) { init_com->c.push_back(pc);  }
    Condition *init_cond = new Condition(init_com, sm->GetCCN(),sm);
    sm->AddCondition(init_cond);

    //Create second state aka exit state to wait
    //until parent switches its state
    State *exit_s = new State(ACT_CHP_SKIP, sm->GetSize(),sm);
    Condition *exit_s_cond = new Condition(exit_s, sm->GetSN(), sm);
    sm->AddCondition(exit_s_cond);
    sm->AddSize();

    //Add exit state to the init state
    n.first = exit_s;
    n.second = init_cond;
    s->AddNextState(n);

    //Processing assigned variable as well as all other
    //variables used in the assigned expression
    Data *d = NULL;
    ActId *var_id = chp_lang->u.assign.id;
    Expr *e = chp_lang->u.assign.e;
    ValueIdx *var_vx = var_id->rootVx(scope);

    int var_dims = 0;
    int var_w = 0;
    ihash_bucket *hb;
    act_booleanized_var_t *bv;
    act_dynamic_var_t *dv;
    act_connection *var_con;
    Variable *nv;

    dv = BOOL->isDynamicRef(bnl, var_id);
    if (!dv) {
      var_con = var_id->Canonical(scope);
      hb = ihash_lookup(bnl->cH, (long)var_con);
      bv = (act_booleanized_var_t *)hb->v;
      var_w = bv->width;
    } else {
      var_con = dv->id;
      var_w = dv->width;
    }

    if (is_declared(tsm, var_con) == 0) {
      nv = new Variable(0, 0, var_vx, var_con);
      nv->AddDimension(var_w-1);
      if (dv) {
        nv->MkDyn();
      }
      tsm->AddVar(nv);
    }

    d = new Data(0,0,0, proc, tsm, init_cond, NULL, var_id, e);
    tsm->AddData(var_con, d);

    std::vector<ActId *> var_col;
    collect_vars(e, var_col);
    for (auto v : var_col) {
      var_dims = 0;
      ValueIdx *evar_vx = v->rootVx(scope);
      act_connection *evar_con;

      dv = BOOL->isDynamicRef(bnl, v);
      if (!dv) {
        evar_con = v->Canonical(scope);
        hb = ihash_lookup(bnl->cH, (long)evar_con);
        bv = (act_booleanized_var_t *)hb->v;
      } else {
        evar_con = dv->id;
      }

      int decl_type = is_declared(tsm, evar_con);
      int var_veri_type = decl_type == 0 ? 0 : 1;
    
      if (decl_type == 0 || decl_type == 3) {
        if (dv) {
          var_w = dv->width;
        } else if (bv) {
          var_w = bv->width;
        }
        nv = new Variable (var_veri_type, 0, evar_vx,evar_con);
        nv->AddDimension(var_w-1);
        if (dv) {
          nv->MkDyn();
          for (auto i = 0; i < dv->a->nDims(); i++) {
            nv->AddDimension(dv->a->range_size(i));
          }
        } else if (bv) {
          if (v->arrayInfo()) {
            Array *var_a = v->arrayInfo();
            InstType *it = scope->FullLookup(v, &var_a);
            var_a = it->arrayInfo();
            for (auto i = 0; i < var_a->nDims(); i++) {
              int dim_size = var_a->range_size(i);
              nv->AddDimension(dim_size-1);
              var_dims++;
            }
          }
        }
        tsm->AddVar(nv);
      }
    }

    //Return condition
    if (pc) {
      Comma *npar_com = new Comma();
      npar_com->type = 2;
      npar_com->c.push_back(pc);
      Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
      sm->AddCondition(npar_cond);
      if (is_sc == 0) {
        Comma *full_com  = new Comma();
        full_com->type = 1;
        full_com->c.push_back(exit_s_cond);
        full_com->c.push_back(npar_cond);
        Condition *full_ret_cond = new Condition(full_com, sm->GetCCN(), sm);
        sm->AddCondition(full_ret_cond);
        n.first = s;
        n.second = full_ret_cond;
      } else {
        n.first = s;
        n.second = npar_cond;
      }
      exit_s->AddNextState(n);
    }

    //Termination condition
    Comma *term_com = new Comma();
    term_com->type = 1;
//TODO: this is wating for the optimization :)
//    term_com->c.push_back(init_cond);
    term_com->c.push_back(exit_s_cond);
    Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
    sm->AddCondition(term_cond);

    return term_cond;

    break;
  }
  case ACT_CHP_SEND: {

    std::pair<State *, Condition *> n;
    list_t *l;
    listitem_t *li;

    //Create initial state and corresponding condition
    State *s = NULL;
    s = new State(ACT_CHP_SEND, 0, sm);
    sm->SetFirstState(s);
    Condition *zero_state_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_state_cond);

    //Create communication completion condition
    ActId *chan_id;
    chan_id = chp_lang->u.comm.chan->Canonical(scope)->toid();
    Condition *hs_compl;
    hs_compl = new Condition(chan_id, sm->GetCN(), sm);
    sm->AddCondition(hs_compl);
      
    Condition *commu_compl;
    Comma *commu_compl_com;
    if (pc) {
      commu_compl_com = new Comma();
      commu_compl_com->type = 0;
      commu_compl_com->c.push_back(pc);
      commu_compl_com->c.push_back(hs_compl);
      commu_compl = new Condition(commu_compl_com, sm->GetCCN(), sm);
      sm->AddCondition(commu_compl);
    } else {
      commu_compl = hs_compl;
    }

    //Create initial condition when both parent 
    //and child are in the right state
    Comma *init_com = new Comma;
    init_com->type = 0;
    init_com->c.push_back(zero_state_cond);
    if (pc) { init_com->c.push_back(pc); }
    Condition *init_cond = new Condition(init_com, sm->GetCCN(), sm);
    sm->AddCondition(init_cond);

    //Create initial switching condition when
    //both parent and child are in the right state
    //and communication complete
    Comma *exit_com = new Comma;
    exit_com->type = 0;
    exit_com->c.push_back(commu_compl);
    Condition *exit_cond;
    exit_cond = new Condition(exit_com, sm->GetCCN(), sm);
    sm->AddCondition(exit_cond);

    //Create second state aka exit state to wait
    //until parent switches its state
    State *exit_s = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
    Condition *exit_s_cond = new Condition(exit_s, sm->GetSN(), sm);
    sm->AddCondition(exit_s_cond);
    sm->AddSize();

    //Add exit state to the init state
    n.first = exit_s;
    n.second = exit_cond;
    s->AddNextState(n);

    //Processing channel by finding its direction
    //and bitwidth in the booleanize data structure
    //as well as declaring all undeclared variables 
    //used in the channel send list
    Data *d = NULL;
    act_connection *chan_con;
    chan_con = chan_id->Canonical(scope);
    ValueIdx *chan_vx = chan_id->rootVx(scope);

    int chan_w = 0;
    ihash_bucket_t *hb;
    act_booleanized_var_t *bv;
    act_dynamic_var_t *dv;

    hb = ihash_lookup(bnl->cH, (long)chan_con);
    bv = (act_booleanized_var_t *)hb->v;
    chan_w = bv->width;

    Variable *cv;
    if (is_declared(tsm, chan_con) == 3) {
      cv = new Variable(0, chan_w-1, chan_vx, chan_con);
      tsm->AddVar(cv);
    } else if (is_declared(tsm, chan_con) == 2 ||
                is_declared(tsm, chan_con) == 5) {
      cv = new Variable(0, chan_w-1, 1, chan_vx, chan_con);
      tsm->AddVar(cv);
    }

    std::vector<ActId *> var_col;
    if (chp_lang->u.comm.e) {
      
        Expr *vex = NULL;
        vex = chp_lang->u.comm.e;
        if (vex) {
          collect_vars(vex, var_col);
        }

        Variable *nv = NULL;
        for (auto v : var_col) {
          int var_w = 0;
          ValueIdx *evar_vx = v->rootVx(scope);

          act_connection *evar_con = NULL;
          dv = BOOL->isDynamicRef(bnl, v);
          if (!dv) {
            evar_con = v->Canonical(scope);
            hb = ihash_lookup(bnl->cH, (long)evar_con);
            bv = (act_booleanized_var_t *)hb->v;
          } else {
            evar_con = dv->id;
          }

          int decl_type = is_declared(tsm, evar_con);
          int var_veri_type = decl_type == 0 ? 1 : 0;
          
          if (decl_type == 0 || decl_type == 4) {
            if (dv) {
              var_w = dv->width;
            } else if (bv) {
              var_w = bv->width;
            }
            nv = new Variable(var_veri_type, 0, evar_vx,
                                evar_con);
            nv->AddDimension(var_w-1);
            if (dv) {
              nv->MkDyn();
              for (auto i = 0; i < dv->a->nDims(); i++) {
                nv->AddDimension(dv->a->range_size(i));
              }
            }
            tsm->AddVar(nv);
          }
        }
      
        d = new Data (2, 0, 0, proc, tsm, exit_cond, 
                                      init_cond, chan_id, vex);
        tsm->AddData(chan_con, d);
        tsm->AddHS(chan_con, d);

    } else {
      Expr *dex = NULL;
      char tmp1[1024];
      char tmp2[1024];
      chan_id->sPrint(tmp1, 1024);
      for (auto pp : tsm->GetPorts()) {
        pp->GetCon()->toid()->sPrint(tmp2, 1024);
        if (strcmp(tmp1, tmp2) == 0) {
          pp->SetCtrlChan();
        }
      }
      d = new Data (2, 0, 0, proc, tsm, exit_cond,
                                    init_cond, chan_id, dex);
      tsm->AddHS(chan_con, d);
    }

    //Return to initial state condition is when parent
    //machine leaves current state
 //   if (pc) {
 //     Comma *npar_com = new Comma();
 //     npar_com->type = 2;
 //     npar_com->c.push_back(pc);
 //     Condition *npar_cond = new Condition(npar_com,sm->GetCCN(), sm);
 //     sm->AddCondition(npar_cond);
 //     
 //     n.first = s;
 //     n.second = npar_cond;
 //     exit_s->AddNextState(n);
 //   }
    if (pc) {
      Comma *npar_com = new Comma();
      npar_com->type = 2;
      npar_com->c.push_back(pc);
      Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
      sm->AddCondition(npar_cond);
      if (is_sc == 0) {
        Comma *full_com  = new Comma();
        full_com->type = 1;
        full_com->c.push_back(exit_s_cond);
        full_com->c.push_back(npar_cond);
        Condition *full_ret_cond = new Condition(full_com, sm->GetCCN(), sm);
        sm->AddCondition(full_ret_cond);
        n.first = s;
        n.second = full_ret_cond;
      } else {
        n.first = s;
        n.second = npar_cond;
      }
      exit_s->AddNextState(n);
    }

    //Terminate condition is when recv machine is in
    //the exit state or when communication completion
    //is actually happening i.e. hand shake is valid
    Comma *term_com = new Comma();
    term_com->type = 1;
//TODO: This speed up is dangerous without dependency analisys
//    term_com->c.push_back(commu_compl);
    term_com->c.push_back(exit_s_cond);
    Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
    sm->AddCondition(term_cond);

    return term_cond;

    break;
  }
  case ACT_CHP_RECV: {

    std::pair<State *, Condition *> n;
    list_t *l;
    listitem_t *li;

    //Create initial state and corresponding condition
    State *s = NULL;
    s = new State(ACT_CHP_RECV, 0, sm);
    sm->SetFirstState(s);
    Condition *zero_state_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_state_cond);

 //   //Create communication completion condition
 //   ActId *chan_id;
 //   chan_id = chp_lang->u.comm.chan->Canonical(scope)->toid();
 //   Condition *commu_compl;
 //   commu_compl = new Condition(chan_id, sm->GetCN(), sm);
 //   sm->AddCondition(commu_compl);
    //Create communication completion condition
    ActId *chan_id;
    chan_id = chp_lang->u.comm.chan->Canonical(scope)->toid();
    Condition *hs_compl;
    hs_compl = new Condition(chan_id, sm->GetCN(), sm);
    sm->AddCondition(hs_compl);
      
    Condition *commu_compl;
    Comma *commu_compl_com;
    if (pc) {
      commu_compl_com = new Comma();
      commu_compl_com->type = 0;
      commu_compl_com->c.push_back(pc);
      commu_compl_com->c.push_back(hs_compl);
      commu_compl = new Condition(commu_compl_com, sm->GetCCN(), sm);
      sm->AddCondition(commu_compl);
    } else {
      commu_compl = hs_compl;
    }


    //Create initial condition when both parent 
    //and child are in the right state
    Comma *init_com = new Comma;
    init_com->type = 0;
    init_com->c.push_back(zero_state_cond);
    if (pc) { init_com->c.push_back(pc); }
    Condition *init_cond = new Condition(init_com, sm->GetCCN(), sm);
    sm->AddCondition(init_cond);
    //Create initial switching condition when
    //both parent and child are in the right state
    //and communication complete
    Comma *exit_com = new Comma;
    exit_com->type = 0;
    exit_com->c.push_back(commu_compl);
    Condition *exit_cond;
    exit_cond = new Condition(exit_com, sm->GetCCN(), sm);
    sm->AddCondition(exit_cond);
    //Create second state aka exit state to wait
    //until parent switches its state
    State *exit_s = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
    Condition *exit_s_cond = new Condition(exit_s, sm->GetSN(), sm);
    sm->AddCondition(exit_s_cond);
    sm->AddSize();

    //Add exit state to the init state
    n.first = exit_s;
    n.second = exit_cond;
    s->AddNextState(n);

    //Processing channel by finding its direction
    //and bitwidth in the booleanize data structure
    //as well as declaring all undeclared variables 
    //used in the channel recv list
    Data *d = NULL;
    act_connection *chan_con;
    chan_con = chan_id->Canonical(scope);
    ValueIdx *chan_vx = chan_id->rootVx(scope);

    int chan_w = 0;
    ihash_bucket_t *hb;
    act_booleanized_var_t *bv;
    hb = ihash_lookup(bnl->cH, (long)chan_con);
    bv = (act_booleanized_var_t *)hb->v;
    chan_w = bv->width;

    if (is_declared(tsm, chan_con) == 4) {
      Variable *cv = new Variable(1, chan_w-1, chan_vx,chan_con);
      tsm->AddVar(cv);
    }

    if (chp_lang->u.comm.var) {
      ActId *var_id = NULL;
      act_connection *var_con = NULL;
      var_id = chp_lang->u.comm.var;
      ValueIdx *var_vx = var_id->rootVx(scope);
      
      act_dynamic_var_t *dv;
      act_booleanized_var_t *bv;
      
      int var_w = 0;
      dv = BOOL->isDynamicRef(bnl, var_id);
      if (!dv) {
        var_con = var_id->Canonical(scope);
        hb = ihash_lookup(bnl->cH, (long)var_con);
        bv = (act_booleanized_var_t *)hb->v;
      } else {
        var_con = dv->id;
      }
        
      int decl_type = is_declared(tsm,var_con);
      int var_veri_type = decl_type == 0 ? 0 : 1;
      
      Variable *nv = NULL;

      if (decl_type == 0 || decl_type == 3) {
        if (dv) {
          var_w = dv->width;
        } else if (bv) {
          var_w = bv->width;
        }
        nv = new Variable(0, var_veri_type, var_vx, var_con);
        nv->AddDimension(var_w-1);
        if (dv) {
          nv->MkDyn();
          for (auto i = 0; i < dv->a->nDims(); i++) {
            nv->AddDimension(dv->a->range_size(i));
          }
        } 
        tsm->AddVar(nv);
      }
      
      //Add data type as receive is basically assignment
      //to the variable
      d = new Data (1, 0, 0, proc, tsm, exit_cond, 
                                    init_cond, var_id, chan_id);
      
      tsm->AddData(var_con, d);
      tsm->AddHS(chan_con, d);
    } else {
      ActId *did = NULL;
      int found = 0;
      char tmp1[1024];
      char tmp2[1024];
      chan_id->sPrint(tmp1, 1024);
      for (auto pp : tsm->GetPorts()) {
        pp->GetCon()->toid()->sPrint(tmp2, 1024);
        if (strcmp(tmp1, tmp2) == 0) {
          pp->SetCtrlChan();
          found = 1;
          break;
        }
      }
      d = new Data(1, 0, 0, proc, tsm, exit_cond,
                                    init_cond, did, chan_id);
      tsm->AddHS(chan_con, d);
    }

    //Return to initial state condition is when parent
    //machine leaves current state
 //   if (pc) {
 //     Comma *npar_com = new Comma();
 //     npar_com->type = 2;
 //     npar_com->c.push_back(pc);
 //     Condition *npar_cond = new Condition(npar_com,sm->GetCCN(), sm);
 //     sm->AddCondition(npar_cond);
 //     n.first = s;
 //     n.second = npar_cond;
 //     exit_s->AddNextState(n);
 //   }
    if (pc) {
      Comma *npar_com = new Comma();
      npar_com->type = 2;
      npar_com->c.push_back(pc);
      Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
      sm->AddCondition(npar_cond);
      if (is_sc == 0) {
        Comma *full_com  = new Comma();
        full_com->type = 1;
        full_com->c.push_back(exit_s_cond);
        full_com->c.push_back(npar_cond);
        Condition *full_ret_cond = new Condition(full_com, sm->GetCCN(), sm);
        sm->AddCondition(full_ret_cond);
        n.first = s;
        n.second = full_ret_cond;
      } else {
        n.first = s;
        n.second = npar_cond;
      }
      exit_s->AddNextState(n);
    }

    //Terminate condition is when recv machine is in
    //the exit state or when communication completion
    //is actually happening i.e. hand shake is valid
    Comma *term_com = new Comma();
    term_com->type = 1;
    //term_com->c.push_back(commu_compl);
//TODO: This speed up is dangerous without dependency analisys
//    term_com->c.push_back(exit_cond);
    term_com->c.push_back(exit_s_cond);
    Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
    sm->AddCondition(term_cond);
    return term_cond;

    break;
  }
  case ACT_CHP_FUNC: {
    fprintf(stdout, "FUNC : not sure I know what to do with this\n");
    break;
  }
  case ACT_CHP_SEMILOOP: {
    fprintf(stdout, "SEMILOOP should be expanded\n");
    break;
  }
  case ACT_CHP_COMMALOOP: {
    fprintf(stdout, "COMMALOOP should be expanded\n");
    break;
  }
  default: {
    fprintf(stdout, "Unknown chp type: %d\n", chp_lang->type);
    break;
  }
  }
  return NULL;
}

//Adding process ports
void add_ports(Scope *cs, act_boolean_netlist_t *bnl, StateMachine *sm){
  ActId *tmp_id;
  ValueIdx *tmp_v;
  act_connection *tmp_c;
  unsigned int tmp_d = 0;
  unsigned int tmp_w = 0;
  unsigned int chan = 0;
  ihash_bucket *hb;
  int reg = 1;
  for (auto i = 0; i < A_LEN(bnl->chpports); i++) {
    if (bnl->chpports[i].omit) { continue; }
    reg = 1;
    tmp_id = bnl->chpports[i].c->toid();
    tmp_v = tmp_id->rootVx(cs);
    tmp_c = tmp_id->Canonical(cs);
    tmp_d = bnl->chpports[i].input;

    hb = ihash_lookup(bnl->cH, (long)tmp_c);
    act_booleanized_var_t *bv;
    bv = (act_booleanized_var_t *)hb->v;

    tmp_w = bv->width;
    chan = bv->ischan;

    for (auto in : sm->GetInst()){
      for (auto pp : in->GetPorts()) {
        if (pp->GetVx() == tmp_v) {
          reg = 0;
          break;
        }
      }
    }

    Port *p = new Port(tmp_d, tmp_w, chan, reg, tmp_v, tmp_c);
    sm->AddPort(p);
  }
  for (auto i = 0; i < A_LEN(bnl->used_globals); i++) {
    tmp_id = bnl->used_globals[i]->toid();
    tmp_v = tmp_id->rootVx(cs);
    tmp_c = tmp_id->Canonical(cs);
    tmp_d = 1;
    tmp_w = 1;

    reg = 1;
    for (auto in : sm->GetInst()){
      for (auto pp : in->GetPorts()) {
        if (pp->GetVx() == tmp_v) {
          reg = 0;
          break;
        }
      }
    }

    Port *p = new Port(tmp_d, tmp_w, 0, reg, tmp_v, tmp_c);
    sm->AddPort(p);
  }
}

//Map machine instances to their origins
void map_instances(CHPProject *cp){
  for (auto pr0 = cp->Head(); pr0; pr0 = pr0->GetNext()) {
    for (auto inst : pr0->GetInst()) {
      for (auto pr1 = cp->Head(); pr1; pr1 = pr1->GetNext()) {
        if (inst->GetProc() == pr1->GetProc()) {
          inst->SetSM(pr1);
          for (auto i = 0; i < pr1->GetPorts().size(); i++) {
            if (pr1->GetPorts()[i]->GetChan() == 2) {
              inst->SetCtrlChan(i);
            }
          }
        }
      }
    }
  }
}

//Adding all instances in the current process
void add_instances(Scope *cs, act_boolean_netlist_t *bnl, StateMachine *sm){

  ActUniqProcInstiter i(cs);
  
  StateMachineInst *smi;
  
  int iport = 0;
  
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (BOOL->getBNL (dynamic_cast<Process *>(vx->t->BaseType()))->isempty) {
      continue;
    }
      
    act_boolean_netlist_t *sub;
    sub = BOOL->getBNL (dynamic_cast<Process *>(vx->t->BaseType()));
      
    int ports_exist = 0;
    for (int j = 0; j < A_LEN(sub->chpports); j++) {
      if (sub->chpports[j].omit == 0) {
	ports_exist = 1;
	break;
      }
    }
    if (ports_exist == 1) {
      if (vx->t->arrayInfo()) {
	Arraystep *as = vx->t->arrayInfo()->stepper();
	while (!as->isend()) {
	  if (vx->isPrimary (as->index())) {
	    Process *p = dynamic_cast<Process *>(vx->t->BaseType());
	    char *ar = as->string();
	    std::vector<Port *> ports;
	    for (auto j = 0; j < A_LEN(sub->chpports); j++){
	      if (sub->chpports[j].omit) { continue; }
	      act_connection *c = bnl->instchpports[iport]->toid()->Canonical(cs);
	      ValueIdx *vv = c->toid()->rootVx(cs);           

	      ihash_bucket *hb;
	      hb = ihash_lookup(bnl->cH, (long)c);
	      act_booleanized_var_t *bv;
	      bv = (act_booleanized_var_t *)hb->v;
              
	      int dir = sub->chpports[j].input;
	      int width = bv->width;
	      int ischan = bv->ischan;
              
	      Port *ip = new Port(dir,width,ischan,0,vv, c);
	      ip->SetInst();
	      ports.push_back(ip);
	      iport++;
	    }
	    smi = new StateMachineInst(p,vx,ar,ports);
	    sm->AddInst(smi);
	  }
	  as->step();
	}
      } else {
	Process *p = dynamic_cast<Process *>(vx->t->BaseType());
	char *ar = NULL;
	std::vector<Port *> ports;
	for (auto j = 0; j < A_LEN(sub->chpports); j++){
	  if (sub->chpports[j].omit) { continue; }
	  act_connection *c = bnl->instchpports[iport]->toid()->Canonical(cs);
	  ValueIdx *vv = c->toid()->rootVx(cs);
            
	  ihash_bucket *hb;
	  hb = ihash_lookup(bnl->cH, (long)c);
	  act_booleanized_var_t *bv;
	  bv = (act_booleanized_var_t *)hb->v;
            
	  int dir = sub->chpports[j].input;
	  int width = bv->width;
	  int ischan = bv->ischan;
            
	  Port *ip = new Port(dir,width,ischan,0,vv,c);
	  ip->SetInst();
	  ports.push_back(ip);
	  iport++;
	}
	smi = new StateMachineInst(p,vx,ar,ports);
	sm->AddInst(smi);
      }
    }
  }
}

//Function to find instance to instance connections
//which need to be declared
void inst_to_inst_con_decl(Scope *cs, StateMachine *sm) {

  act_boolean_netlist_t *bnl = BOOL->getBNL(sm->GetProc());

  for (auto iv : sm->GetInst()) {
    for (auto ip : iv->GetPorts()) {
      int decl_type = is_declared(sm, ip->GetCon());
      if (decl_type == 0 || decl_type == 3 || decl_type == 4) {
        act_connection *var_con = ip->GetCon();
        ValueIdx *var_vx = var_con->toid()->rootVx(cs);
        ihash_bucket *hb = ihash_lookup(bnl->cH, (long)var_con);
        act_booleanized_var_t *bv = (act_booleanized_var_t *)hb->v;
        int var_w = bv->width;
        int var_chan = bv->ischan;
        Variable *nv = new Variable(1, var_chan, 0, var_vx, var_con);
        nv->AddDimension(var_w-1);
        sm->AddVar(nv);
      }
    }
  }

}

//Function to traverse act data strcutures and walk
//through entire act project hierarchy while building
//state machine for each process
void traverse_act (Process *p, CHPProject *cp) {

  act_boolean_netlist_t *bnl = BOOL->getBNL(p);

  if (bnl->visited) {
    return;
  }
  bnl->visited = 1;

  if (bnl->isempty) {
    return;
  }

  act_languages *lang;
  act_chp *chp;
  Scope *cs;

  if (p) {
    lang = p->getlang();
    cs = p->CurScope();
  }

  if (lang) {
    chp = lang->getchp();
  }

  ActInstiter i(cs);

  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType(vx->t)) {
      traverse_act (dynamic_cast<Process *>(vx->t->BaseType()), cp);
    }
  }


  act_chp_lang_t *chp_lang = NULL;
  if (chp) {
    chp_lang = chp->c;
  }


  //Create state machine for the currect
  //process
  StateMachine *sm = new StateMachine();
  sm->SetProcess(p);
  sm->SetNumber(0);

  //add instances
  add_instances(cs, bnl, sm);

  //add ports
  add_ports(cs, bnl, sm);

  //run chp traverse to build state machine
  if (chp_lang) {
    traverse_chp(p, chp_lang, sm, sm, NULL);
  }

  //add interconnections
  inst_to_inst_con_decl(cs, sm);

  //append linked list of chp project
  //processes
  cp->Append(sm);
}

CHPProject *build_machine (Act *a, Process *p) {

  ActPass *apb = a->pass_find("booleanize");

  BOOL = dynamic_cast<ActBooleanizePass *>(apb);

  CHPProject *cp = new CHPProject();
  traverse_act (p, cp);
  map_instances(cp);

  return cp;
}

}
