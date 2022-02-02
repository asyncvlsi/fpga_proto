#include <act/state_machine.h>
#include <act/passes/booleanize.h>
#include <act/iter.h>

/*
ACT_CHP_COMMA = 0
ACT_CHP_SEMI = 1
ACT_CHP_SELECT = 2
ACT_CHP_SELECT_NONDET = 3
ACT_CHP_LOOP = 4
ACT_CHP_DOLOOP = 5
ACT_CHP_SKIP = 6
ACT_CHP_ASSIGN = 7
ACT_CHP_SEND = 8
ACT_CHP_RECV = 9
ACT_CHP_FUNC = 10
ACT_CHP_SEMILOOP = 11
ACT_CHP_COMMALOOP = 12
ACT_CHP_HOLE = 13
ACT_CHP_ASSIGNSELF = 14
ACT_CHP_MACRO = 15
ACT_HSE_FRAGMENTS = 16

ACT_CHP_INF_LOOP = 17
*/

#define ACT_CHP_INF_LOOP 17

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

//Function to find state dependencies
//returns True if dependency is found
bool find_dep(act_chp_lang_t *chp_lang, 
              act_connection *vc)
{

  switch (chp_lang->type) {
  case ACT_CHP_COMMA: {

    list_t *l;
    listitem_t *li;
    act_chp_lang_t *cl;

    break; 
  }
  case ACT_CHP_SEMI:  { break; }
  case ACT_CHP_SELECT:{ break; }
  case ACT_CHP_SELECT_NONDET: { break; }
  case ACT_CHP_LOOP: { break; }
  case ACT_CHP_DOLOOP: { break; }
  case ACT_CHP_ASSIGN: { break; }
  case ACT_CHP_SEND:    { break; }
  case ACT_CHP_RECV:    { break; }
  default:
    return false;
  }

  return false;
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
                        int par_chp,        //parent chp type
                        int opt             //optimization level
                        ) {
  Scope *scope;
  scope = proc->CurScope();
  act_boolean_netlist_t *bnl = BOOL->getBNL(proc);
  Condition *tmp = NULL;

  switch (chp_lang->type) {
  case ACT_CHP_COMMA: {

    //Comma type completion means concurrent completion of 
    //all comma'ed statements. If parent condition exists
    //then execution is synchronized by pc otherwise a 
    //dummy state is created at the higher level(e.g. first 
    //statement in the SEMI chain) 

    std::pair<State *, Condition *> n;
    std::vector<Condition *> vc;

    list_t *l;
    listitem_t *li;
    act_chp_lang_t *cl;

    l = chp_lang->u.semi_comma.cmd;

    Condition *child_cond;
    if (pc) {
      child_cond = pc;
    } else {
      child_cond = NULL;
    }

    //traverse all COMMAed statements
    for (li = list_first(l); li; li = list_next(li)) {

      cl = (act_chp_lang_t *)list_value(li);

      //ignore skip and func statements
      if (cl->type == ACT_CHP_SKIP || 
          cl->type == ACT_CHP_FUNC) {
        continue;
      }

      //if statement is COMMA then no new sm
      //is needed otherwise create new child sm
      //TODO: check if a comma inside a comma is possible :)
      if (cl->type == ACT_CHP_COMMA || cl->type == ACT_CHP_SEMI) {
        tmp = traverse_chp(proc, cl, sm, tsm, child_cond, ACT_CHP_COMMA, opt);
        sm->AddCondition(tmp); //TODO: check this. Not sure yet
      } else {
        if (sm->IsEmpty()) {
          tmp = traverse_chp(proc, cl, sm, sm , child_cond, ACT_CHP_COMMA, opt);
        } else {
          StateMachine *csm = new StateMachine();
          csm->SetNumber(sm->GetKids());
          csm->SetParent(sm);
          csm->SetProcess(sm->GetProc());
          tmp = traverse_chp(proc, cl, csm, tsm , child_cond, ACT_CHP_COMMA, opt);
          if (tmp) {
            sm->AddKid(csm);
          } else {
            csm = NULL;
            delete csm;
          }
        }
      }

      //add child termination condition to the termination  
      //vector
      if (tmp) {
        vc.push_back(tmp);
      }

    }

    //create general termination conditon using
    //ANDed child termination conditions, i.e.
    //child_term1 & child_term2 & ... & child_termN
    if (vc.size() > 0) {
      Comma *term_com = new Comma;
      term_com->type = 0;
      term_com->c = vc;
      Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
      return term_cond;
    } else {
      return NULL;
    }

    break;
  }
  case ACT_CHP_SEMI: {

    //Semi is a chain of statements. Every statement
    //execution is intiated by the termination condition
    //of its predecessor with an exception for the very
    //first statement which is initiated by the parent
    //condition. If semi is a top statement then first
    //statement in the list is a top level machine

    std::pair<State *, Condition *> n;

    list_t *l = NULL;
    listitem_t *li = NULL;
    act_chp_lang_t *cl = NULL;

    l = chp_lang->u.semi_comma.cmd;

    Condition *child_cond = NULL;

    int first_skip = 0;

    Comma *first_com = new Comma();
    first_com->type = 0;
    Condition *first_cond = NULL;

    Comma *tmp_com = new Comma();
    Condition *tmp_cond = NULL;

    Comma *term_com = new Comma();
    term_com->type = 0;

    //traverse all statements separated with semicolon
    for (li = list_first(l); li; li = list_next(li)) {

      cl = (act_chp_lang_t *)list_value(li);

      //ignore skip and func statements
      if (cl->type != ACT_CHP_SKIP & cl->type != ACT_CHP_FUNC) {

        //if the first statement was skipped then pc should be used
        //as initial condition for the first non-skip statement
        if (li == list_first(l) || 
            (((act_chp_lang_t *)(list_value(list_first(l))))->type == ACT_CHP_SKIP ||
             ((act_chp_lang_t *)(list_value(list_first(l))))->type == ACT_CHP_FUNC )
             & first_skip == 0) {
          first_skip = 1;
          if (opt >= 2 & (par_chp == ACT_CHP_LOOP | 
                          par_chp == ACT_CHP_INF_LOOP)) {
            tmp_com->type = 2;
            tmp_cond = new Condition (tmp_com, sm->GetCCN(), sm);
            sm->AddCondition(tmp_cond);
            first_com->type = 0;
            first_com->c.push_back(tmp_cond);
            if (pc) {
              first_com->c.push_back(pc);
            }
            first_cond = new Condition(first_com, sm->GetCCN(), sm);
            sm->AddCondition(first_cond);
            child_cond = first_cond;
          } else {
            if (pc) {
              child_cond = pc;
            } else {
              child_cond = NULL;
            }
          }
        } else {
          if (opt >= 1) {
            Comma *child_com = new Comma();
            child_com->type = 0;
            if (pc) {
              child_com->c.push_back(pc);
            }
            child_com->c.push_back(tmp);
            child_cond = new Condition(child_com, sm->GetCCN(), sm);
            sm->AddCondition(child_cond);
          } else {
            child_cond = tmp;
          }
        }

        //If next statement is semi then do nothing
        if (cl->type == ACT_CHP_SEMI) {
          tmp = traverse_chp(proc, cl, sm, tsm, child_cond, ACT_CHP_SEMI, opt);
          sm->AddCondition(tmp);
        //If next statement is comma and current level is 
        //the top of chp then comma should be synchronized
        //by creating a dummy state otherwise follow general
        //case scenario
        } else if (cl->type == ACT_CHP_COMMA) {
          if (!pc & !child_cond) {
            State *dum_st = new State(ACT_CHP_COMMA, 0, sm);
            sm->SetFirstState(dum_st);
            Condition *zero_s_cond = new Condition(dum_st, sm->GetSN(), sm);
            sm->AddCondition(zero_s_cond);
            child_cond = zero_s_cond;
          }
          tmp = traverse_chp(proc, cl, sm, tsm, child_cond, ACT_CHP_SEMI, opt);
          sm->AddCondition(tmp);
        //The rest statement cases
        } else {
          if (sm->IsEmpty()) {
            tmp = traverse_chp(proc, cl, sm, sm, child_cond, ACT_CHP_SEMI, opt);
          } else {
            StateMachine *csm = new StateMachine();
            csm->SetNumber(sm->GetKids());
            csm->SetParent(sm);
            csm->SetProcess(sm->GetProc());
            tmp = traverse_chp(proc, cl, csm, tsm, child_cond, ACT_CHP_SEMI, opt);
            if (tmp) {
              sm->AddKid(csm);
            } else {
              csm = NULL;
              delete csm;
            }
          }
        }

        //if valid condition returned - replace old valid
        //termination condition
        if (tmp) {
          if (term_com->c.size() > 0) {
            term_com->c.pop_back();
          }
          term_com->c.push_back(tmp);
        }
      }
    }

    //Create termination condition (last valid child term condition) 
    Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
    if (opt >= 2 & (par_chp == ACT_CHP_LOOP | par_chp == ACT_CHP_INF_LOOP)) {
      tmp_com->c.push_back(term_cond);
    }

    return term_cond;
    
    break;
  }
  case ACT_CHP_SELECT: {

    //Selection is a control statement. It waits until a guard
    //is true and executes selected branch. Its completion 
    //is determined by the completion of the selected branch.

    std::pair<State *, Condition *> n;
    std::vector<Condition *> vc;
    std::vector<Condition *> ve;

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

    //Flags for cases when parametarized select turns out
    //to be useless
    //[ pbool(false) -> bla bla
    //[]else -> skip
    //] -> nothing going to happen
    int empty_select = 0;
    int else_flag = 0;

    //Check if the selection statement is empty
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
      
        //If guard is not NULL
        if (gg->g) {
          if (gg->s && gg->s->type != ACT_CHP_SKIP
                    && gg->s->type != ACT_CHP_FUNC) {
            guard = new Condition(gg->g, sm->GetGN(), sm);
            sm->AddCondition(guard);
            ve.push_back(guard);
          } else {
            continue;
          }
        //Guard is NULL then it is an else statement
        } else {
          if (gg->s->type == ACT_CHP_FUNC) {
            char tmp[1024] = "log";
            if (strcmp(tmp, gg->s->u.func.name->s) != 0){
              fprintf(stdout, "%s\n", tmp);
              fatal_error("I don't know this function");
            }
          }
          guard = NULL;
          else_flag = 1;
        }

        //If statement is not NULL
        if (gg->s) {
          //Create dummy skip state for skip and func statements
          //with a valid guard
          if (gg->s->type != ACT_CHP_FUNC ||
              gg->s->type != ACT_CHP_SKIP) {
            ss = new State(gg->s->type, sm->GetSize(), sm);
          } else {
            ss = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
          }
          sm->AddSize();

          Comma *guard_com = new Comma;
          guard_com->type = 0;
          guard_com->c.push_back(zero_s_cond);
          if (pc) { guard_com->c.push_back(pc); }

          //Create conditions(full guard) to switch to the execution
          //states using initial condition and corresponding
          //guards. If guard is NULL (else) use else vector
          //equal to all guards being false as a secondary 
          //switching condition
          if (else_flag == 1) {
            if (ve.size() > 0) {
              Comma *else_com = new Comma;
              else_com->type = 2;
              else_com->c = ve;
              Condition *else_guard;
              else_guard = new Condition(else_com, sm->GetCCN(), sm);
              sm->AddCondition(else_guard);
              guard_com->c.push_back(else_guard);
            }
          } else {
            guard_com->c.push_back(guard);
          }

          Condition *full_guard;
          full_guard = new Condition(guard_com, sm->GetCCN(), sm);
          n.first = ss;
          n.second = full_guard;
          s->AddNextState(n);
          sm->AddCondition(full_guard);

          //Create child conditions using execution states
          Condition *tmp_cond = NULL;
          if (pc) {
            Comma *child_com = new Comma;
            child_com->type = 0;
            tmp_cond = new Condition(ss, sm->GetSN(), sm);
            sm->AddCondition(tmp_cond);
            child_com->c.push_back(tmp_cond);
            if (opt >= 1) {
              child_com->c.push_back(pc);
            }
            child_cond = new Condition(child_com, sm->GetCCN(), sm);
          } else {
            child_cond = new Condition(s, sm->GetSN(), sm);
          }
          sm->AddCondition(child_cond);
          //Traverse the rest of the hierarchy
          if (gg->s->type == ACT_CHP_COMMA || gg->s->type == ACT_CHP_SEMI) {
            tmp = traverse_chp(proc, gg->s, sm, tsm, child_cond, ACT_CHP_SELECT, opt);
            sm->AddCondition(tmp);
          //If statement if skip of func then use execution state
          //as a termination condition
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
            tmp = traverse_chp(proc, gg->s, csm, tsm, child_cond, ACT_CHP_SELECT, opt);
            if (tmp) {
              sm->AddKid(csm);
            } else {
              csm = NULL;
              delete csm;
            }
          }
          if (tmp) {
            vc.push_back(tmp);
          } else {
            vc.push_back(child_cond);
          }
        }
      }

      //Create new condition to switch to the exit state
      //after execution completion. Use ORed child terminations
      //conditions
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

      //TODO: this is for later
      //Comma *term_com = new Comma;
      //term_com->type = 1;
      //term_com->c = vc; 
      //Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);

      //Create termination condition which is an exit state
      Comma *term_com = new Comma;
      term_com->type = 1;
      term_com->c.push_back(exit_s_cond); 
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

      //Return NULL on the empty selection statement
      return NULL;

    }

    break;
  }
  case ACT_CHP_SELECT_NONDET: {
    //Selection is a control statement. It waits until at least
    //one guard is true and executes selected branch. Its completion 
    //is determined by the completion of the selected branch.

    std::pair<State *, Condition *> n;
    std::vector<Condition *> vc;
    std::vector<Condition *> ve;

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

    //Check if the selection statement is empty
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
     
        //No NULL guard is possible unlike in the DET SELECTION
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
        }
      
        if (gg->s) {
          ss = new State(gg->s->type, sm->GetSize(), sm);
          sm->AddSize();
      
          Comma *guard_com = new Comma;
          guard_com->type = 0;
          guard_com->c.push_back(zero_s_cond);
          if (pc) { guard_com->c.push_back(pc); }

          guard_com->c.push_back(guard);

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
            tmp_cond = new Condition(ss, sm->GetSN(), sm);
            sm->AddCondition(tmp_cond);
            child_com->c.push_back(tmp_cond);
            if (opt >= 1) {
              child_com->c.push_back(pc);
            }
            child_cond = new Condition(child_com, sm->GetCCN(), sm);
          } else {
            child_cond = new Condition(s, sm->GetSN(), sm);
          }
          sm->AddCondition(child_cond);
      
          if (gg->s->type == ACT_CHP_COMMA || gg->s->type == ACT_CHP_SEMI) {
            tmp = traverse_chp(proc, gg->s, sm, tsm, child_cond, ACT_CHP_SELECT_NONDET, opt);
          } else {
            StateMachine *csm = new StateMachine();
            csm->SetNumber(sm->GetKids());
            csm->SetParent(sm);
            csm->SetProcess(sm->GetProc());
            tmp = traverse_chp(proc, gg->s, csm, tsm, child_cond, ACT_CHP_SELECT_NONDET, opt);
            if (tmp) {
              sm->AddKid(csm);
            } else {
              csm = NULL;
              delete csm;
            }
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
      //TODO: this is for later
      //Comma *term_com = new Comma;
      //term_com->type = 1;
      //term_com->c = vc;
      //Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);

      //Create termination condition which is an exit state
      Comma *term_com = new Comma;
      term_com->type = 1;
      term_com->c.push_back(exit_s_cond); 
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

      return NULL;

    }

    break;
  }
  case ACT_CHP_LOOP: {

    //Loop type statement keeps executing while at least one guard
    //is true. Termination condition is AND of all guards being false.

    std::pair<State *, Condition *> n;
    std::vector<Condition *> vc;
    std::vector<Condition *> vt;

    int inf_flag = 0;

    //Create initial state and corresponding state condition
    State *s = NULL;
    s = new State(ACT_CHP_LOOP, 0, sm);
    sm->SetFirstState(s);
    Condition *zero_s_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_s_cond);

    Condition *child_cond = NULL;
    //Traverse all branches
    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {

      Condition *g = NULL;
      State *ss = NULL;

      //If loop is infinite there is no guard and guard
      //condition is replaced with the zero state condition
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
      vt.push_back(g);

      ss = new State(gg->s->type, sm->GetSize(), sm);
      sm->AddSize();
      n.first = ss;

      //If loop is not infinite use guard as a part of the
      ///switching condition
      //If there is no parent condition then using zero state
      //and corresponding guard is enough
      Comma *par_com = new Comma();
      par_com->type = 0;
      if (g != zero_s_cond) {
        par_com->c.push_back(g);
      }
      if (pc) {
        par_com->c.push_back(pc);
      }
      par_com->c.push_back(zero_s_cond);
      Condition *par_cond = new Condition(par_com, sm->GetCCN(), sm);
      n.second = par_cond;
      vc.push_back(par_cond);
      sm->AddCondition(par_cond);
      s->AddNextState(n);

      //Create child condition
      if (pc) {
        Comma *child_com = new Comma;
        child_com->type = 0;
        Condition *tmp_cond = new Condition(ss, sm->GetSN(), sm);
        sm->AddCondition(tmp_cond);
        if (opt >= 2 & inf_flag == 0) {
          Comma *tmp_com = new Comma();
          tmp_com->type = 0;
          tmp_com->c.push_back(tmp_cond);
          tmp_com->c.push_back(pc);
          child_com->type = 1;
          child_com->c.push_back(par_cond);
          tmp_cond = new Condition(tmp_com, sm->GetCCN(), sm);
          sm->AddCondition(tmp_cond);
          child_com->c.push_back(tmp_cond);
        } else if (opt >= 1) {
          child_com->c.push_back(tmp_cond);
          child_com->c.push_back(pc);
        } else {
          child_com->c.push_back(tmp_cond);
        }
        child_cond = new Condition(child_com, sm->GetCCN(), sm);
      } else {
        child_cond = new Condition(ss, sm->GetSN(), sm);
      }
      sm->AddCondition(child_cond);

      //Traverse lower levels of CHP hierarchy
      if (gg->s->type == ACT_CHP_COMMA || gg->s->type == ACT_CHP_SEMI) {
        if (inf_flag == 0) {
          tmp = traverse_chp(proc, gg->s, sm, tsm, child_cond, ACT_CHP_LOOP, opt);
        } else {
          tmp = traverse_chp(proc, gg->s, sm, tsm, child_cond, ACT_CHP_INF_LOOP, opt);
        }
        sm->AddCondition(tmp);
      } else {
        StateMachine *csm = new StateMachine();
        csm->SetNumber(sm->GetKids());
        csm->SetParent(sm);
        csm->SetProcess(sm->GetProc());
        if (inf_flag == 0) {
          tmp = traverse_chp(proc, gg->s, csm, tsm, child_cond, ACT_CHP_LOOP, opt);
        } else {
          tmp = traverse_chp(proc, gg->s, sm, tsm, child_cond, ACT_CHP_INF_LOOP, opt);
        }
        if (tmp) {
          sm->AddKid(csm);
        } else {
          csm = NULL;
          delete csm;
        }
      }

      //Create iteration condition which is a termination
      //condition of the branch statement
      if (inf_flag == 0 && tmp) {
        if (opt == 2 && gg->s->type != ACT_CHP_COMMA) {
          Comma *loop_com = new Comma;
          loop_com->type = 0;
          loop_com->c.push_back(tmp);
          Condition *loop_c = new Condition(loop_com, sm->GetCCN(), sm);
          n.first = s;
          n.second = loop_c;
          sm->AddCondition(loop_c);
          ss->AddNextState(n);
        } else {
          Comma *loop_com = new Comma;
          loop_com->type = 0;
          loop_com->c.push_back(tmp);
          Condition *loop_c = new Condition(loop_com, sm->GetCCN(), sm);
          n.first = s;
          n.second = loop_c;
          sm->AddCondition(loop_c);
          ss->AddNextState(n);
        }
      }
    }

    //create condition for all negative guards
    Comma *nguard_com = new Comma;
    nguard_com->type = 2;
    nguard_com->c = vt;
    Condition *nguard_cond = new Condition(nguard_com, sm->GetCCN(), sm);
    sm->AddCondition(nguard_cond);

    //create general loop termination condition
    //when loop is in the zero state and all guards
    //are false
    Comma *exit_com = new Comma();
    exit_com->c.push_back(zero_s_cond);
    exit_com->c.push_back(nguard_cond);
    if (pc) {exit_com->c.push_back(pc); }
    exit_com->type = 0;
    Condition *exit_cond = new Condition(exit_com, sm->GetCCN(), sm);
    sm->AddCondition(exit_cond);

    //Create exit state
    State *exit_s = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
    sm->AddSize();
    n.first = exit_s;
    n.second = exit_cond;

    Condition *exit_s_cond = new Condition(exit_s, sm->GetSN(), sm);
    sm->AddCondition(exit_s_cond);

    s->AddNextState(n);

    //Create condition to return to the zero state
    //If parent condition exists then its negation is the condition
    //If loop is infinite then exit condition is the condition, 
    //however it is created just for the consistency of the model
    //and never actually used.
    if (pc || inf_flag == 1) {
      Comma *npar_com = new Comma;
      if (pc) {
        npar_com->type = 2;
        npar_com->c.push_back(pc);
      } else {
        npar_com->type = 0;
        npar_com->c.push_back(exit_cond);
      }
      Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
      sm->AddCondition(npar_cond);

      n.first = s;
      n.second = npar_cond;
      exit_s->AddNextState(n);
    }

    //Use exit state as a termination condition
    Comma *term_com = new Comma;
    term_com->type = 1;
    term_com->c.push_back(exit_s_cond);
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
  case ACT_CHP_ASSIGN: {

    //Assignment is a state machine with two states.
    //First is IDLE and EXECUTION combined. It waits
    //till parent is in the right state to 
    //evaluate expression and switch to the EXIT state
    //at the next clock cycle. Exit state is 
    //a termination condition

    std::pair<State *, Condition *> n;

    //Create initial state and corresponding condition
    State *s = NULL;
    s = new State(ACT_CHP_ASSIGN, 0, sm);
    sm->SetFirstState(s);
    Condition *zero_s_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_s_cond);

    //Create initial condition when both parent 
    //and child are in the right state. This is when
    //the assignment takes place
    Comma *init_com = new Comma();
    init_com->type = 0;
    init_com->c.push_back(zero_s_cond);
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
    //variables used in the assigned expression. This is 
    //necessary for the declaration section is Verilog
    //module
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
        Array *a = dv->a;
        for (auto i = 0; i < a->nDims(); i++) {
          nv->AddDimension(a->range_size(i));
        }
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

    //Create termination condition using an exit state condition
    Comma *term_com = new Comma();
    term_com->type = 1;
    //TODO: this is for later
    //term_com->c.push_back(init_cond);
    term_com->c.push_back(exit_s_cond);
    Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
    sm->AddCondition(term_cond);

    //Create a return condition as a negation of the parent
    //condition or as a more optimized version return back
    //right after switching to exit state
    if (pc) {
      if (par_chp == ACT_CHP_LOOP | par_chp == ACT_CHP_INF_LOOP |
          par_chp == ACT_CHP_SELECT | par_chp == ACT_CHP_SELECT_NONDET
          & opt == 2) {
        n.first = s;
        n.second = term_cond;
        exit_s->AddNextState(n);
      } else {
        Comma *npar_com = new Comma();
        npar_com->type = 2;
        npar_com->c.push_back(pc);
        Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
        sm->AddCondition(npar_cond);
        n.first = s;
        n.second = npar_cond;
        exit_s->AddNextState(n);
      }
    }
 
    return term_cond;

    break;
  }
  case ACT_CHP_SEND: {

    //Send is a communication statement. It includes two
    //states. First is an IDLE state which waits for the
    //parent machine to switch to the corresponding state.
    //When both send and parent machines are in the right
    //state valid/ready signal is set high and waits for
    //the communication to complete, i.e. ready and valid
    //signal of the the same channel to be high. When 
    //communication is over (1 clock cycle) send machine
    //switches to the EXIT state.

    std::pair<State *, Condition *> n;
    list_t *l;
    listitem_t *li;

    //Create initial state and corresponding condition
    State *s = NULL;
    s = new State(ACT_CHP_SEND, 0, sm);
    sm->SetFirstState(s);
    Condition *zero_s_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_s_cond);

    //Create handshaking condition
    ActId *chan_id;
    chan_id = chp_lang->u.comm.chan->Canonical(scope)->toid();
    Condition *hs_compl;
    hs_compl = new Condition(chan_id, sm->GetCN(), sm);
    sm->AddCondition(hs_compl);

    //Create communication completion condition
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
    //and child are in the right state to set valid/ready high
    Comma *init_com = new Comma;
    init_com->type = 0;
    init_com->c.push_back(zero_s_cond);
    if (pc) { init_com->c.push_back(pc); }
    Condition *init_cond = new Condition(init_com, sm->GetCCN(), sm);
    sm->AddCondition(init_cond);

    //Create exit switching condition when
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
      cv->AddDimension(chan_w-1);
      tsm->AddVar(cv);
    } else if (is_declared(tsm, chan_con) == 2 ||
                is_declared(tsm, chan_con) == 5) {
      cv = new Variable(0, chan_w-1, 1, chan_vx, chan_con);
      cv->AddDimension(chan_w-1);
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

    //Create return condition when parent
    //machine switches from the current state
    if (pc) {
      Comma *npar_com = new Comma();
      npar_com->type = 2;
      npar_com->c.push_back(pc);
      Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
      sm->AddCondition(npar_cond);
      n.first = s;
      n.second = npar_cond;
      exit_s->AddNextState(n);
    }

    //Terminate condition is when send machine is in
    //the exit state 
    Comma *term_com = new Comma();
    term_com->type = 1;
    //TODO: This is for later
    if (opt >= 2) {
      term_com->c.push_back(commu_compl);
    }
    term_com->c.push_back(exit_s_cond);
    Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);
    sm->AddCondition(term_cond);

    return term_cond;

    break;
  }
  case ACT_CHP_RECV: {

    //Recv is a communication statement. It includes two
    //states. First is an IDLE state which waits for the
    //parent machine to switch to the corresponding state.
    //When both Recv and parent machines are in the right
    //state valid/ready signal is set high and waits for
    //the communication to complete, i.e. ready and valid
    //signal of the the same channel to be high. When 
    //communication is over (1 clock cycle) Recv machine
    //switches to the EXIT state.

    std::pair<State *, Condition *> n;
    list_t *l;
    listitem_t *li;

    //Create initial state and corresponding condition
    State *s = NULL;
    s = new State(ACT_CHP_RECV, 0, sm);
    sm->SetFirstState(s);
    Condition *zero_s_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_s_cond);

    //Create handshaking condition
    ActId *chan_id;
    chan_id = chp_lang->u.comm.chan->Canonical(scope)->toid();
    Condition *hs_compl;
    hs_compl = new Condition(chan_id, sm->GetCN(), sm);
    sm->AddCondition(hs_compl);
      
    //Create communication completion condition
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
    init_com->c.push_back(zero_s_cond);
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
    act_dynamic_var_t *dv;

    hb = ihash_lookup(bnl->cH, (long)chan_con);
    bv = (act_booleanized_var_t *)hb->v;
    chan_w = bv->width;

    Variable *cv;
    if (is_declared(tsm, chan_con) == 4) {
      cv = new Variable(1, chan_w-1, chan_vx,chan_con);
      cv->AddDimension(chan_w-1);
      tsm->AddVar(cv);
    }

    if (chp_lang->u.comm.var) {
      ActId *var_id = NULL;
      act_connection *var_con = NULL;
      var_id = chp_lang->u.comm.var;
      ValueIdx *var_vx = var_id->rootVx(scope);
      
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

    //Create return condition when parent
    //machine switches from the current state
    if (pc) {
      Comma *npar_com = new Comma();
      npar_com->type = 2;
      npar_com->c.push_back(pc);
      Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
      sm->AddCondition(npar_cond);
      n.first = s;
      n.second = npar_cond;
      exit_s->AddNextState(n);
    }

    //Terminate condition is when recv machine is in
    //the exit state
    Comma *term_com = new Comma();
    term_com->type = 1;
    //TODO: This is for later
    //term_com->c.push_back(commu_compl);
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

        act_dynamic_var_t *dv = BOOL->isDynamicRef(bnl, var_con->toid());

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
void traverse_act (Process *p, CHPProject *cp, int opt) {

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

  ActUniqProcInstiter i(cs);

  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    traverse_act (dynamic_cast<Process *>(vx->t->BaseType()), cp, opt);
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
    traverse_chp(p, chp_lang, sm, sm, NULL, 0, opt);
  }

  //add interconnections
  inst_to_inst_con_decl(cs, sm);

  //append linked list of chp project
  //processes
  cp->Append(sm);
}

CHPProject *build_machine (Act *a, Process *p, int opt) {

  ActPass *apb = a->pass_find("booleanize");

  BOOL = dynamic_cast<ActBooleanizePass *>(apb);

  CHPProject *cp = new CHPProject();
  traverse_act (p, cp, opt);
  map_instances(cp);

  return cp;
}

}
