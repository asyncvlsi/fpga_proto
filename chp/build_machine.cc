#include <act/state_machine.h>
#include <act/passes/booleanize.h>
#include <act/iter.h>

namespace fpga {

static ActBooleanizePass *BOOL = NULL;

//Returns true if chp type is simple
//fasle if complex. In case of complex 
//statements a new child state machine
//is created
bool is_simple(int type) {
  if (type == ACT_CHP_COMMA) {
    return true;
  } else {
    return false;
  }
}

//Function to check whether variable is added to the list
//or not.
//0 - not declared
//1 - declared as variable
//2 - declared as port
//3 - declared as instance port input
//4 - declared as instance port output
int is_declared (StateMachine *sm, act_connection *v) {
  std::vector<Variable *> vv;
  vv = sm->GetVars();
  for (auto iv : vv) {
    if (iv->GetCon() == v) { return 1; }
  }
  std::vector<Port *> vp;
  vp = sm->GetPorts();
  for (auto iv : vp) {
    if (iv->GetCon() == v) { return 2; }
  }
  std::vector<StateMachineInst *> iv;
	iv = sm->GetInst();
	for (auto in : iv) {
		for (auto ip : in->GetPorts()) {
			if (ip->GetCon() == v && ip->GetDir() == 1) {
				return 3;
			} else if (ip->GetCon() == v && ip->GetDir() == 0) {
				return 4;
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
                        Condition *pc       //parent init cond
                        ) {

  Scope *scope;
  scope = proc->CurScope();
  act_boolean_netlist_t *bnl = BOOL->getBNL(proc);

  Condition *tmp;

  switch (chp_lang->type) {

  case ACT_CHP_COMMA: {

  	std::pair<State *, Condition *> n;
  	std::vector<Condition *> vc;

	  list_t *l;
  	listitem_t *li;
  	act_chp_lang_t *cl;

    //Comma type completions means concurrent completion of 
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
      if (cl->type == ACT_CHP_SKIP) { continue; }

      //if statement is simple then no new sm
      //is needed otherwise create new child sm
      if (is_simple(cl->type)) {
        tmp = traverse_chp(proc, cl, sm, tsm, child_cond);
      } else {
        StateMachine *csm = new StateMachine();
        csm->SetNumber(sm->GetKids());
        csm->SetParent(sm);
				csm->SetProcess(sm->GetProc());
        sm->AddKid(csm);
        tmp = traverse_chp(proc, cl, csm, tsm , child_cond);
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

    //if there is no parent sm the create dummy
    //SKIP state and after termination switch to it
    if (s) {
      State *exit_s = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
      n.first = exit_s;
      n.second = term_cond;
      s->AddNextState(n);
      sm->AddCondition(term_cond);
    }

    return term_cond;

    break;
  }
  case ACT_CHP_SEMI: {

  	std::pair<State *, Condition *> n;

	  list_t *l;
  	listitem_t *li;
  	act_chp_lang_t *cl;

    //Semi type completion means completion of the statement
    //between two semicolons.

    l = chp_lang->u.semi_comma.cmd;

    State *s = NULL;
    State *first_s = NULL;
    State *prev_s = NULL;

    Condition *prev_cond = NULL;
    Condition *next_cond = NULL;
    Condition *child_cond = NULL;

    Comma *next_com = NULL;
    //traverse all statements separated with semicolon
    for (li = list_first(l); li; li = list_next(li)) {

      cl = (act_chp_lang_t *)list_value(li);

      //if statement is SKIP then simply ignore it 
      if (cl->type != ACT_CHP_SKIP) {

        Comma *next_com = new Comma;
        next_com->type = 0;

        //semicolon is a complex statement and every
        //child statement is a new state
        s = new State(cl->type, sm->GetSize(), sm);

        //child condition is the current state
				if (pc) {
					Comma *child_com = new Comma;
					child_com->type = 0;
					child_com->c.push_back(pc);
					Condition *tmp_cond = new Condition(s, sm->GetSN(), sm);
					sm->AddCondition(tmp_cond);
					child_com->c.push_back(tmp_cond);
					child_cond = new Condition(child_com, sm->GetCCN(), sm);
				} else {
	        child_cond = new Condition(s, sm->GetSN(), sm);
				}
        sm->AddCondition(child_cond);

        //if semicolon is the top level statement then
        //add first statement to the current sm
        if (sm->IsEmpty()) {
          sm->SetFirstState(s);
        } else {
          sm->AddSize();
        }

        //save first statement for futher plans 
        if (li == list_first(l)) {
          first_s = s;
        }

        //if child statement is simple then no new sm
        //is needed otherwise create new child sm    
        if (is_simple(cl->type)) {
          tmp = traverse_chp(proc, cl, sm, tsm, child_cond);
  
          //if simple statement returns condition then we
          //need to store it in the current sm
          if (tmp) {
            next_com->c.push_back(tmp);
            sm->AddCondition(tmp);
          }
        } else {
          StateMachine *csm = new StateMachine();
          csm->SetNumber(sm->GetKids());
          csm->SetParent(sm);
					csm->SetProcess(sm->GetProc());
          sm->AddKid(csm);
          tmp = traverse_chp(proc, cl, csm, tsm, child_cond);
          next_com->c.push_back(tmp);
        }

        std::pair<State *, Condition *> n;

        //checking if not first child statement is processed
        //and add it as the next state to the previous state
        if (prev_s) {
          n.first = s;
          n.second = prev_cond;
          prev_s->AddNextState(n);
        }
  
        //add child condition to the general switch
        //condition
        next_com->c.push_back(child_cond);

        next_cond = new Condition(next_com, sm->GetCCN(), sm);

        //store current state as a previous one for the next
        prev_s = s;
        prev_cond = next_cond;

        sm->AddCondition(next_cond);
      }

    }

		//Create exit state to wait until parent switches to
		//another state
    State *exit_s = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
		sm->AddSize();
    n.first = exit_s;
    n.second = next_cond;
    prev_s->AddNextState(n);

		Condition *exit_s_cond = new Condition(exit_s, sm->GetSN(), sm);
		sm->AddCondition(exit_s_cond);

		//Create condition to return to the initial state
		//after parent is no more in the right state
    if (pc) {
			Comma *npar_com = new Comma;
			npar_com->type = 2;
			npar_com->c.push_back(pc);
			Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
			sm->AddCondition(npar_cond);
      n.first = first_s;
      n.second = npar_cond;
      exit_s->AddNextState(n);
		}

		//Terminate condition is when last statement is
		//complete or the machine is in the exit state
		Comma *term_com = new Comma;
		term_com->type = 1;
		term_com->c.push_back(exit_s_cond);
		term_com->c.push_back(next_cond);
		Condition *term_cond = new Condition(term_com, sm->GetCCN(),sm);
		sm->AddCondition(term_cond);
    return term_cond;
    
    break;
  }
  case ACT_CHP_SELECT: {

    std::pair<State *, Condition *> n;
  	std::vector<Condition *> vc;
    //Selection statement completion happens after completion of
    //execution of one if the selction options. Thus in case of
    //return state existance function returns ORed comma condition
    //with options completion condiitons.

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

    //Process all selection options
    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {

      if (gg->g) {
        if (gg->s && gg->s->type != ACT_CHP_SKIP) {
          guard = new Condition(gg->g, sm->GetGN(), sm);
          sm->AddCondition(guard);
        } else {
					continue;
				}
      } else {
				continue;
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

        if (is_simple(gg->s->type)) {
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

    return term_cond;

    break;
  }
  case ACT_CHP_SELECT_NONDET: {

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {
      Condition *g = new Condition(gg->g, sm->GetGN(), sm);
      sm->AddCondition(g);
      traverse_chp (proc, chp_lang->u.gc->s, sm, tsm, NULL);
    }
    break;
  }
  case ACT_CHP_LOOP: {

		std::pair<State *, Condition *> n;
  	std::vector<Condition *> vc;
  	std::vector<Condition *> vt;

    //Loop type states keep executing while at least one guard
    //stays true. Termination condition is AND of all all guards
    //with negation.

    int inf_flag = 0;

    //Create initial state and corresponding state condition
    State *s = NULL;
    s = new State(ACT_CHP_LOOP, 0, sm);
    sm->SetFirstState(s);
    Condition *zero_s_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_s_cond);

    Condition *child_cond = NULL;

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {

      Condition *g = NULL;
      State *ss = NULL;

      //If loop is infinite there is no guard
      if (gg->g) {
        if (gg->s->type != ACT_CHP_SKIP) {
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

      //if there is no parent machine then simply
      //use guard as the next state condition
      //if parent machine exists then need to add
      //parent condition to guarantee correct sequence
      //of the events
      if (!pc) {
        n.second = g;
      } else {
        Comma *par_com = new Comma();
        par_com->type = 0;
        if (g != zero_s_cond) {
          par_com->c.push_back(g);
        }
        par_com->c.push_back(pc);
        par_com->c.push_back(zero_s_cond);
        Condition *par_cond = new Condition(par_com, sm->GetCCN(), sm);
        n.second = par_cond;
        vc.push_back(par_cond);
        sm->AddCondition(par_cond);
      }
      s->AddNextState(n);

			if (pc) {
				Comma *child_com = new Comma;
				child_com->type = 0;
				child_com->c.push_back(pc);
				Condition *tmp_cond = new Condition(ss, sm->GetSN(), sm);
				sm->AddCondition(tmp_cond);
				child_com->c.push_back(tmp_cond);
				child_cond = new Condition(child_com, sm->GetCCN(), sm);
			} else {
	      child_cond = new Condition(ss, sm->GetSN(), sm);
			}
      sm->AddCondition(child_cond);

      if (is_simple(gg->s->type)) {
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
        Comma *loop_com = new Comma;
        loop_com->type = 0;
        loop_com->c.push_back(tmp);
        Condition *loop_c = new Condition(loop_com, sm->GetCCN(), sm);
        n.first = s;
        n.second = loop_c;
        sm->AddCondition(loop_c);
      } else {
        n.first = s;
        n.second = child_cond;
      }
      ss->AddNextState(n);
    }

    //create condition for all negative guards
    Comma *nguard_com = new Comma;
    nguard_com->type = 2;
    nguard_com->c = vt;
    Condition *nguard_cond = new Condition(nguard_com, sm->GetCCN(), sm);
    sm->AddCondition(nguard_cond);

    //create general loop termination condition
    Comma *exit_com = new Comma();
    exit_com->c.push_back(zero_s_cond);
    exit_com->c.push_back(nguard_cond);
		if (pc) {exit_com->c.push_back(pc); }
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

		Comma *term_com = new Comma;
		term_com->type = 1;
		term_com->c.push_back(exit_s_cond);
		term_com->c.push_back(exit_cond);
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
		if (pc) { init_com->c.push_back(pc);	}
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

    std::string sid;
    char buf[1024];
    var_id->sPrint(buf, 1024);
    sid = buf;

    int var_w = 0;
    ihash_bucket *hb;
    act_booleanized_var_t *bv;
    act_connection *var_con;
		var_con = var_id->Canonical(scope);
    hb = ihash_lookup(bnl->cH, (long)var_con);
    bv = (act_booleanized_var_t *)hb->v;
    if (bv->ischan == 1 || bv->isint == 1) {
      var_w = bv->width;
    } else {
      var_w = 1;
    }

    Variable *nv;
    if (is_declared(tsm, var_con) == 0) {
      nv = new Variable(0, var_w-1, 0, var_con);
      tsm->AddVar(nv);
    }

    std::vector<ActId *> var_col;
    collect_vars(e, var_col);
    for (auto v : var_col) {
      act_connection *evar_con = v->Canonical(scope);
      hb = ihash_lookup(bnl->cH, (long)evar_con);
      bv = (act_booleanized_var_t *)hb->v;

			int decl_type = is_declared(tsm, evar_con);

			int var_veri_type = decl_type == 0 ? 0 : 1;

      if (decl_type == 0 || decl_type == 3) {
        if (bv->ischan == 1 || bv->isint == 1) {
          nv = new Variable(var_veri_type,bv->width-1, 0,evar_con);
        } else {
          nv = new Variable(var_veri_type,0,0,evar_con);
        }
        tsm->AddVar(nv);
      }
    }

    d = new Data(0, var_w-1,0, proc, tsm, init_cond, NULL, var_id, e);
    tsm->AddData(sid, d);

		//Return condition
   	if (pc) {
			Comma *npar_com = new Comma();
			npar_com->type = 2;
			npar_com->c.push_back(pc);
			Condition *npar_cond = new Condition(npar_com, sm->GetCCN(), sm);
			n.first = s;
			n.second = npar_cond;
			exit_s->AddNextState(n);
			sm->AddCondition(npar_cond);
		}

		//Termination condition
		Comma *term_com = new Comma();
		term_com->type = 1;
		term_com->c.push_back(init_cond);
		term_com->c.push_back(exit_s_cond);
		Condition *term_cond = new Condition(term_com, sm->GetSN(), sm);
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
		Condition *commu_compl;
    commu_compl = new Condition(chan_id, sm->GetCN(), sm);
		sm->AddCondition(commu_compl);

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
		exit_com->c.push_back(init_cond);
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

    std::string sid;
    char buf[1024];
    chan_id->sPrint(buf,1024);
    sid = buf;

    int chan_w = 0;
    ihash_bucket_t *hb;
    act_booleanized_var_t *bv;
    hb = ihash_lookup(bnl->cH, (long)chan_con);
    bv = (act_booleanized_var_t *)hb->v;
    chan_w = bv->width;

		if (is_declared(tsm, chan_con) == 3) {
			Variable *rv = new Variable(0, chan_w-1, 1, chan_con);
			tsm->AddVar(rv);
		}

    std::vector<ActId *> var_col;
    l = chp_lang->u.comm.rhs;
    for (li = list_first(l); li; li = list_next(li)) {

      Expr *vex = NULL;
      vex = (Expr *)list_value(li);
      collect_vars(vex, var_col);

      for (auto v : var_col) {

        act_connection *var_con = v->Canonical(scope);
        hb = ihash_lookup(bnl->cH, (long)var_con);
        bv = (act_booleanized_var_t *)hb->v;

				int decl_type = is_declared(tsm,var_con);
				int var_veri_type = decl_type == 0 ? 1 : 0;

        Variable *nv;
				if (decl_type == 0 || decl_type == 4) {
        	if (bv->isint == 1 || bv->ischan == 1) {
        	  nv = new Variable(var_veri_type, bv->width-1, 0, var_con);
        	} else {
        	  nv = new Variable(var_veri_type, 0, 0, var_con);
        	}
        	tsm->AddVar(nv);
				}
      }
		
      d = new Data (2, bv->width-1, 0, proc, tsm, exit_cond, 
																				init_cond, chan_id, vex);
      tsm->AddData(sid, d);

      var_col.clear();

    }

		//Return to initial state condition is when parent
		//machine leaves current state
		if (pc) {
			Comma *npar_com = new Comma();
			npar_com->type = 2;
			npar_com->c.push_back(pc);
			Condition *npar_cond = new Condition(npar_com,sm->GetCCN(), sm);
			sm->AddCondition(npar_cond);
    	
			n.first = s;
			n.second = npar_cond;
			exit_s->AddNextState(n);
		}

		//Terminate condition is when recv machine is in
		//the exit state or when communication completion
		//is actually happening i.e. hand shake is valid
		Comma *term_com = new Comma();
		term_com->type = 1;
		term_com->c.push_back(commu_compl);
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

		//Create communication completion condition
    ActId *chan_id;
    chan_id = chp_lang->u.comm.chan->Canonical(scope)->toid();
		Condition *commu_compl;
    commu_compl = new Condition(chan_id, sm->GetCN(), sm);
		sm->AddCondition(commu_compl);

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
		exit_com->c.push_back(init_cond);
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

    int chan_w = 0;
    ihash_bucket_t *hb;
    act_booleanized_var_t *bv;
    hb = ihash_lookup(bnl->cH, (long)chan_con);
    bv = (act_booleanized_var_t *)hb->v;
    chan_w = bv->width;

		if (is_declared(tsm, chan_con) == 4) {
			Variable *rv = new Variable(1, chan_w-1, 1, chan_con);
			tsm->AddVar(rv);
		}

    l = chp_lang->u.comm.rhs;
    for (li = list_first(l); li; li = list_next(li)) {

      ActId *var_id = NULL;
      act_connection *var_con = NULL;
      var_id = (ActId *)list_value(li);
      var_con = var_id->Canonical(scope);

      hb = ihash_lookup(bnl->cH, (long)var_con);
      act_booleanized_var_t *bv;
      bv = (act_booleanized_var_t *)hb->v;

			int decl_type = is_declared(tsm,var_con);

      Variable *nv;
			if (decl_type == 0 || decl_type == 3) {
      	if (bv->isint) {
      	  nv = new Variable(0, bv->width-1, 0, var_con);
      	} else {
      	  nv = new Variable(0, 0, 0, var_con);
      	}
      	tsm->AddVar(nv);
			}

      std::string sid;
      char buf[1024];
      var_id->sPrint(buf,1024);
      sid = buf;

			//Add data type as receive is basically assignment
			//to the variable
      d = new Data (1, bv->width-1, 0, proc, tsm, exit_cond, 
																		init_cond, var_id, chan_id);
      tsm->AddData(sid, d);
    }

		//Return to initial state condition is when parent
		//machine leaves current state
		if (pc) {
			Comma *npar_com = new Comma();
			npar_com->type = 2;
			npar_com->c.push_back(pc);
			Condition *npar_cond = new Condition(npar_com,sm->GetCCN(), sm);
			sm->AddCondition(npar_cond);
    	
			n.first = s;
			n.second = npar_cond;
			exit_s->AddNextState(n);
		}

		//Terminate condition is when recv machine is in
		//the exit state or when communication completion
		//is actually happening i.e. hand shake is valid
		Comma *term_com = new Comma();
		term_com->type = 1;
		term_com->c.push_back(commu_compl);
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
  act_connection *tmp_c;
  unsigned int tmp_d = 0;
  unsigned int tmp_w = 0;
  unsigned int chan = 0;
  ihash_bucket *hb;
  for (auto i = 0; i < A_LEN(bnl->chpports); i++) {
    if (bnl->chpports[i].omit) { continue; }
    tmp_c = bnl->chpports[i].c->toid()->Canonical(cs);
    tmp_d = bnl->chpports[i].input;

    hb = ihash_lookup(bnl->cH, (long)tmp_c);
    act_booleanized_var_t *bv;
    bv = (act_booleanized_var_t *)hb->v;

    tmp_w = bv->width;
    chan = bv->ischan;

    Port *p = new Port(tmp_d, tmp_w, chan, tmp_c);
    sm->AddPort(p);
  }
  for (auto i = 0; i < A_LEN(bnl->used_globals); i++) {
    bnl->used_globals[i]->toid()->Canonical(cs)->toid()->Print(stdout);
    tmp_c = bnl->used_globals[i]->toid()->Canonical(cs);
    tmp_d = 1;
    tmp_w = 1;
    Port *p = new Port(tmp_d, tmp_w, 0, tmp_c);
    sm->AddPort(p);
    fprintf(stdout, "\n");
  }
}

//Map machine instances to their origins
void map_instances(CHPProject *cp){
	for (auto pr0 = cp->Head(); pr0; pr0 = pr0->GetNext()) {
		for (auto inst : pr0->GetInst()) {
			for (auto pr1 = cp->Head(); pr1; pr1 = pr1->GetNext()) {
				if (inst->GetProc() == pr1->GetProc()) {
					inst->SetSM(pr1);
				}
			}
		}
	}
}

//Adding all instances in the current process
void add_instances(Scope *cs, act_boolean_netlist_t *bnl, StateMachine *sm){

	ActInstiter i(cs);
	
	StateMachineInst *smi;
	
	int iport = 0;
	
	for (i = i.begin(); i != i.end(); i++) {
		ValueIdx *vx = *i;
		if (TypeFactory::isProcessType(vx->t)) {
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
						Process *p = dynamic_cast<Process *>(vx->t->BaseType());
						char *ar = as->string();
						std::vector<Port *> ports;
						for (auto j = 0; j < A_LEN(sub->chpports); j++){
							if (sub->chpports[j].omit) { continue; }
							act_connection *c = bnl->instchpports[iport]->toid()->Canonical(cs);
							
							ihash_bucket *hb;
							hb = ihash_lookup(bnl->cH, (long)c);
							act_booleanized_var_t *bv;
							bv = (act_booleanized_var_t *)hb->v;
							
							int dir = sub->chpports[j].input;
							int width = bv->width;
							int ischan = bv->ischan;
							
							Port *ip = new Port(dir,width,ischan,c);
							ip->SetInst();
							ports.push_back(ip);
							iport++;
						}
						smi = new StateMachineInst(p,vx,ar,ports);
						sm->AddInst(smi);
						as->step();
					}
				} else {
					Process *p = dynamic_cast<Process *>(vx->t->BaseType());
					char *ar = NULL;
					std::vector<Port *> ports;
					for (auto j = 0; j < A_LEN(sub->chpports); j++){
						if (sub->chpports[j].omit) { continue; }
						act_connection *c = bnl->instchpports[iport]->toid()->Canonical(cs);
						
						ihash_bucket *hb;
						hb = ihash_lookup(bnl->cH, (long)c);
						act_booleanized_var_t *bv;
						bv = (act_booleanized_var_t *)hb->v;
						
						int dir = sub->chpports[j].input;
						int width = bv->width;
						int ischan = bv->ischan;
						
						Port *ip = new Port(dir,width,ischan,c);
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
	fprintf(stdout, "//=======================\n");

  act_chp_lang_t *chp_lang = chp->c;

  //Create state machine for the currect
  //process and with the initial state0 
  StateMachine *sm = new StateMachine();
  sm->SetProcess(p);
  sm->SetNumber(0);

  //add ports
  add_ports(cs, bnl, sm);

	//add instances
	add_instances(cs, bnl, sm);

  //run chp traverse to build state machine
  traverse_chp(p, chp_lang, sm, sm, NULL);

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
