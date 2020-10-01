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
  } else if (type == ACT_CHP_SEMI) {
    return false;
  } else if (type == ACT_CHP_SELECT) {
    return false;
  } else if (type == ACT_CHP_SELECT_NONDET) {
    return false;
  } else if (type == ACT_CHP_LOOP) {
    return false;
  } else if (type == ACT_CHP_DOLOOP) {
    return false;
  } else if (type == ACT_CHP_SKIP) {
    return true;
  } else if (type == ACT_CHP_ASSIGN) {
    return true;
  } else if (type == ACT_CHP_SEND) {
    return true;
  } else if (type == ACT_CHP_RECV) {
    return true;
  } else if (type == ACT_CHP_FUNC) {
    return false;
  } else if (type == ACT_CHP_SEMILOOP) {
    return false;
  } else if (type == ACT_CHP_COMMALOOP) {
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
    if (e->u.e.r) {collect_vars(e->u.e.r, vars); }
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

  std::pair<State *, Condition *> n; 
  std::vector<Condition *> vc;
  std::vector<Condition *> vt;
  list_t *l;
  listitem_t *li;
  act_chp_lang_t *cl;

  Condition *tmp;

  switch (chp_lang->type) {

  case ACT_CHP_COMMA: {

    fprintf(stdout, "//COMMA\n");

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
        sm->AddKid(csm);
        tmp = traverse_chp(proc, cl, csm, tsm , child_cond);
      }

      //if valid condition is returned then add it
      //to the termination vector
      if (tmp) {
        vc.push_back(tmp);
        sm->AddCondition(tmp);
      }

    }

    //if comma is top level then need to add
    //child condition to the termination list
    if (s) {
      sm->AddCondition(child_cond);
      vc.push_back(child_cond);
    }

    //creating general termination conditon
    Comma *com = new Comma;
    com->type = 0;
    com->c = vc;
    Condition *term = new Condition(com, sm->GetCCN(), sm);

    //if there is no parent sm the create dummy
    //SKIP state and after termination switch to it
    if (s) {
      State *ss = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
      n.first = ss;
      n.second = term;
      s->AddNextState(n);
      sm->AddCondition(term);
    }

    return term;

    break;
  }
  case ACT_CHP_SEMI: {

    fprintf(stdout, "//SEMI\n");

    //Semi type completion means completion of the statement
    //between two semicolons.

    l = chp_lang->u.semi_comma.cmd;

    State *s = NULL;
    State *first_s = NULL;
    State *prev_s = NULL;

    Condition *prev_cond = NULL;
    Condition *child_cond = NULL;
    Condition *next_cond = NULL;

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

    //when the last statement completes switch to the dummy
		//state and wait until parent sm switches to the next
		//state so that children sm's can switch back to zero
		//states
    State *ds = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
    n.first = ds;
    n.second = next_cond;
		sm->AddSize();
    prev_s->AddNextState(n);
    if (pc) {
			Comma *dum_com = new Comma;
			dum_com->type = 2;
			dum_com->c.push_back(pc);
			Condition *dum_cond = new Condition(dum_com, sm->GetCCN(), sm);
			sm->AddCondition(dum_cond);
      n.first = first_s;
      n.second = dum_cond;
      ds->AddNextState(n);
      return next_cond;
    }
    return tmp;

    break;
  }
  case ACT_CHP_SELECT: {

    fprintf(stdout, "//SELECT\n");
    //Selection statement completion happens after completion of
    //execution of one if the selction options. Thus in case of
    //return state existance function returns ORed comma condition
    //with options completion condiitons.

    //create new state for guard evaluation if needed
    State *s = NULL;
    s = new State(ACT_CHP_SELECT, 0, sm);
    sm->SetFirstState(s);

    Condition *child_cond = NULL;
		Condition *guard = NULL;
		Condition *zero_cond = NULL;
    std::pair<State *, Condition *> n;
    State *ss = NULL;

		zero_cond = new Condition(s, sm->GetSN(), sm);
		sm->AddCondition(zero_cond);

    //walk through all selection options
    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {

      //if guard exists and statement is not skip create
      //new guard condition
      if (gg->g) {
        if (gg->s && gg->s->type != ACT_CHP_SKIP) {
          guard = new Condition(gg->g, sm->GetGN(), sm);
          sm->AddCondition(guard);
        }
      } else {
				continue;
			}

      //is statement exists and it is not skip then create
      //new state
      if (gg->s) {
        if (gg->s->type != ACT_CHP_SKIP) {
          ss = new State(gg->s->type, sm->GetSize(), sm);
          sm->AddSize();

					Comma *guard_com = new Comma;
					guard_com->type = 0;
					guard_com->c.push_back(zero_cond);
					if (pc) { guard_com->c.push_back(pc); }
					guard_com->c.push_back(guard);
					Condition *full_guard = new Condition(guard_com, sm->GetCCN(), sm);
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

          //if statement is simple then no new sm is needed
          //otherwise create new child sm
          if (is_simple(gg->s->type)) {
            tmp = traverse_chp(proc, gg->s, sm, tsm, child_cond);
          } else {
            StateMachine *csm = new StateMachine();
            csm->SetNumber(sm->GetKids());
            csm->SetParent(sm);
            sm->AddKid(csm);
            tmp = traverse_chp(proc, gg->s, csm, tsm, child_cond);
          }
          if (tmp) {
            vc.push_back(tmp);
          }
        }
      }
    }

    //create new general condition of OR type for select
    //statement termination
    Comma *com = new Comma;
    com->type = 1;
    com->c = vc;
    Condition *term = new Condition(com, sm->GetCCN(), sm);
    sm->AddCondition(term);

    //when the last statement completes switch to the dummy
		//state and wait until parent sm switches to the next
		//state so that children sm's can switch back to zero
		//states
    State *ds = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
    n.first = ds;
    n.second = term;
		sm->AddSize();
    ss->AddNextState(n);
    if (pc) {
			Comma *dum_com = new Comma;
			dum_com->type = 2;
			dum_com->c.push_back(pc);
			Condition *dum_cond = new Condition(dum_com, sm->GetCCN(), sm);
			sm->AddCondition(dum_cond);
			n.first = s;
			n.second = dum_cond;
			ds->AddNextState(n);
		}


    return term;

    break;
  }
  case ACT_CHP_SELECT_NONDET: {

    fprintf(stdout, "//NONDET\n");

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {
      Condition *g = new Condition(gg->g, sm->GetGN(), sm);
      sm->AddCondition(g);
      traverse_chp (proc, chp_lang->u.gc->s, sm, tsm, NULL);
    }
    break;
  }
  case ACT_CHP_LOOP: {

    fprintf(stdout, "//LOOP\n");
    //Loop type states keep executing while at least one guard
    //stays true. Termination condition is AND of all all guards
    //with negation.

    int inf_flag = 0;

    //Loop is complex, so always create new first state
    State *s = NULL;
    s = new State(ACT_CHP_LOOP, 0, sm);
    sm->SetFirstState(s);

    //declare child condition for child statements
    Condition *child_cond = NULL;

    //guards are executed at the zero state of the
    //loop, so this one below is it
    Condition *zero_state_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_state_cond);

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {

      //next state is stored in pairs of the state and
      //corresponding condition
      std::pair<State *, Condition *> n;

      Condition *g = NULL;
      State *ss = NULL;

      //if loop is infinite there is no guard
      //that is why checking it first 
      if (gg->g) {

        //if statement is skip then simply ignore
        if (gg->s->type != ACT_CHP_SKIP) {

          //create gaurd condition
          g = new Condition(gg->g, sm->GetGN(), sm);
          sm->AddCondition(g);
        }
      } else {

        //if loop is infinite then set flag to high
        inf_flag = 1;
        g = zero_state_cond;
      }
      //add guard to the terminate vector
      vt.push_back(g);

      //if statement type is skip then ignore
      if (gg->s->type != ACT_CHP_SKIP) {

        //define next state of the corresponding type
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
          if (g != zero_state_cond) {
            par_com->c.push_back(g);
          }
          par_com->c.push_back(pc);
          par_com->c.push_back(zero_state_cond);
          Condition *par_cond = new Condition(par_com, sm->GetCCN(), sm);
          n.second = par_cond;
          vc.push_back(par_cond);
          sm->AddCondition(par_cond);
        }
        s->AddNextState(n);

        //define child condition
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
//        sm->AddCondition(child_cond);

        //if statement is simple then no new sm
        //is needed otherwise create new child sm
        if (is_simple(gg->s->type)) {
          tmp = traverse_chp(proc, gg->s, sm, tsm, child_cond);
        } else {
          StateMachine *csm = new StateMachine();
          csm->SetNumber(sm->GetKids());
          csm->SetParent(sm);
          sm->AddKid(csm);
          tmp = traverse_chp(proc, gg->s, csm, tsm, child_cond);
        }

        //if function returns valid condition value
        //then add it to the common condition for
        //cycle reiteration otherwise use child condition
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
        sm->AddCondition(child_cond);
      }
    }

    //create condition for all guards
    Comma *no_g_com = new Comma;
    no_g_com->type = 2;
    no_g_com->c = vt;
    Condition *no_guard = new Condition(no_g_com, sm->GetCCN(), sm);
    sm->AddCondition(no_guard);

    //create general loop termination condition
    Comma *term_com = new Comma();
    term_com->c.push_back(zero_state_cond);
    term_com->c.push_back(no_guard);
		if (pc) {term_com->c.push_back(pc); }
    term_com->type = 0;
    Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);

    //when the last statement completes switch to the dummy
		//state and wait until parent sm switches to the next
		//state so that children sm's can switch back to zero
		//states
    State *ds = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
    n.first = ds;
    n.second = term_cond;
    s->AddNextState(n);
		sm->AddSize();
    sm->AddCondition(term_cond);
    if (pc && inf_flag == 1) {
			Condition *dum_cond;
			if (pc) {
				Comma *dum_com = new Comma;
				dum_com->type = 2;
				dum_com->c.push_back(pc);
				dum_cond = new Condition(dum_com, sm->GetCCN(), sm);
			} else {
				dum_cond = new Condition(ds, sm->GetSN(), sm);
			}
      n.first = s;
      n.second = dum_cond;
      ds->AddNextState(n);
      sm->AddCondition(dum_cond);
    }

    return term_cond;

    break;
  }
  case ACT_CHP_DOLOOP: {
    fprintf(stdout, "//DOLOOP\n");
    break;
  }
  case ACT_CHP_SKIP: {
    fprintf(stdout, "//SKIP\n");
    return NULL;
    break;
  }
  case ACT_CHP_ASSIGN: {

    fprintf(stdout, "//ASSIGN\n");

    //adding state condition, since assignment
    //takes only one cycle.
    State *s = NULL;
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_ASSIGN, 0, sm);
      sm->SetFirstState(s);
    }

    if (s) {
      tmp = new Condition(s, sm->GetSN(), sm);
    }


    Data *d = NULL;
    ActId *id = chp_lang->u.assign.id;
    Expr *e = chp_lang->u.assign.e;

    std::string sid;
    char buf[1024];
    id->sPrint(buf, 1024);
    sid = buf;

    int wc = 0;
    ihash_bucket *hb;
    act_booleanized_var_t *bv;
    act_connection *idc = id->Canonical(scope);
    hb = ihash_lookup(bnl->cH, (long)idc);
    bv = (act_booleanized_var_t *)hb->v;
    if (bv->ischan == 1 || bv->isint == 1) {
      wc = bv->width;
    } else {
      wc = 1;
    }

    Variable *nv;
    if (is_declared(tsm, idc) == 0) {
      nv = new Variable(0, wc-1, 0, idc);
      tsm->AddVar(nv);
    }

    std::vector<ActId *> var_col;
    collect_vars(e, var_col);
    for (auto v : var_col) {
      act_connection *cur_con = v->Canonical(scope);
      hb = ihash_lookup(bnl->cH, (long)cur_con);
      bv = (act_booleanized_var_t *)hb->v;

      if (is_declared(tsm, cur_con) == 0) {
        if (bv->ischan == 1 || bv->isint == 1) {
          nv = new Variable(0,bv->width-1, 0,cur_con);
        } else {
          nv = new Variable(0,0,0,cur_con);
        }
        tsm->AddVar(nv);
      } else if (is_declared(tsm, cur_con) == 3) {
        if (bv->ischan == 1 || bv->isint == 1) {
          nv = new Variable(1,bv->width-1,0,cur_con);
        } else {
          nv = new Variable(1,0,0,cur_con);
        }
        tsm->AddVar(nv);
      }
    }

    std::pair<State *, Condition *> n;

    if (!pc) {
      n.first = s;
      n.second = tmp;
      s->AddNextState(n);
      d = new Data(0, wc-1,0, proc, tsm, tmp, NULL, id, e);
      tsm->AddData(sid, d);
      return tmp;
    } else if (pc) {
      d = new Data(0, wc-1, 0, proc, tsm, pc, NULL, id, e);
      tsm->AddData(sid, d);
      return NULL;
    }

    break;
  }
  case ACT_CHP_SEND: {

    fprintf(stdout, "//SEND\n");

    //adding communacation completion condition
    State *s = NULL;
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_SEND, 0, sm);
      sm->SetFirstState(s);
    }

    ActId *id;
    id = chp_lang->u.comm.chan->Canonical(scope)->toid();
    tmp = new Condition(id, sm->GetCN(), sm);

    std::pair<State *, Condition *> n;
    Condition *par_con = NULL;


    if (!pc) {
      n.first = s;
      n.second = tmp;
      s->AddNextState(n);
    } else {
      Comma *par_com = new Comma;
      par_com->type = 0;
      par_com->c.push_back(tmp);
      par_com->c.push_back(pc);
      par_con = new Condition(par_com, sm->GetCCN(), sm);
    }

    Data *d = NULL;
    act_connection *ccon;
    ccon = id->Canonical(scope);
    std::string sid;
    char buf[1024];
    id->sPrint(buf,1024);
    sid = buf;

    int wc = 0;

    std::vector<ActId *> var_col; //collection of variables from
                                  //the send expression

    ihash_bucket_t *hb;
    act_booleanized_var_t *bv;
    hb = ihash_lookup(bnl->cH, (long)ccon);
    bv = (act_booleanized_var_t *)hb->v;
    wc = bv->width;

		if (is_declared(tsm, ccon) == 3) {
			Variable *rv = new Variable(0, wc-1, 1, ccon);
			tsm->AddVar(rv);
		}


    l = chp_lang->u.comm.rhs;
    for (li = list_first(l); li; li = list_next(li)) {

      Expr *vex = NULL;
      vex = (Expr *)list_value(li);
      collect_vars(vex, var_col);
      for (auto v : var_col) {
        act_connection *cur_con = v->Canonical(scope);
        hb = ihash_lookup(bnl->cH, (long)cur_con);
        bv = (act_booleanized_var_t *)hb->v;
        Variable *nv;
        if (is_declared(tsm,cur_con) == 0) {
          if (bv->isint == 1 || bv->ischan == 1) {
            nv = new Variable(0, bv->width-1, 0, cur_con);
          } else {
            nv = new Variable(0, 0, 0, cur_con);
          }
          tsm->AddVar(nv);
        } else if (is_declared(tsm, cur_con) == 4) {
          if (bv->isint == 1 || bv->ischan == 1) {
            Variable *nv = new Variable(1, bv->width-1, 0, cur_con);
          } else {
            Variable *nv = new Variable(1, 0, 0, cur_con);
          }
          tsm->AddVar(nv);
        } else if (is_declared(tsm, cur_con) == 3) {
					fprintf(stdout, "WHAAAAAAAT?!\n");
					exit(1);
      	}
      }

      ActId *main_var;
      main_var = var_col[0];
      act_connection *main_con;
      main_con = main_var->Canonical(scope);
      hb = ihash_lookup(bnl->cH, (long)main_con);
      bv = (act_booleanized_var_t *) hb->v;

      if (!pc) {
        d = new Data (2, bv->width-1, 0, proc, tsm, tmp, tmp, id, vex);
      } else {
        d = new Data (2, bv->width-1, 0, proc, tsm, par_con, pc, id, vex);
      }

      tsm->AddData(sid, d);

      var_col.clear();

    }

    return tmp;

    break;
  }
  case ACT_CHP_RECV: {

    fprintf(stdout, "//RECV\n");

    //adding communacation completion condition
    //TODO: recv of a list of vars is not handeled
    State *s = NULL;
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_RECV, 0, sm);
      sm->SetFirstState(s);
    }

    ActId *id;
    id = chp_lang->u.comm.chan->Canonical(scope)->toid();
    tmp = new Condition(id, sm->GetCN(), sm);

    std::pair<State *, Condition *> n;
    Condition *par_con = NULL;

    if (!pc) {
      n.first = s;
      n.second = tmp;
      s->AddNextState(n);
    } else {
      Comma *par_com = new Comma;
      par_com->type = 0;
      par_com->c.push_back(tmp);
      par_com->c.push_back(pc);
      par_con = new Condition(par_com, sm->GetCCN(), sm);
    }

    Data *d = NULL;
    act_connection *ccon;         //channel connection
    ccon = id->Canonical(scope);  //canonical connection
    int wc = 0;                   //channel total width
    ihash_bucket_t *hb;
    act_booleanized_var_t *bv; //booleanized channel
    hb = ihash_lookup(bnl->cH, (long)ccon);
    bv = (act_booleanized_var_t *)hb->v;
    wc = bv->width;

		if (is_declared(tsm, ccon) == 4) {
			Variable *rv = new Variable(1, wc-1, 1, ccon);
			tsm->AddVar(rv);
		}

    std::tuple<int, int, std::string> key;
 
    l = chp_lang->u.comm.rhs;
    for (li = list_first(l); li; li = list_next(li)) {

      ActId *vid = NULL;
      act_connection *vcon = NULL;
      vid = (ActId *)list_value(li);
      vcon = vid->Canonical(scope);

      hb = ihash_lookup(bnl->cH, (long)vcon);
      act_booleanized_var_t *bv;
      bv = (act_booleanized_var_t *)hb->v;

      Variable *nv;
      if (is_declared(tsm, vcon) == 0) {
        if (bv->isint) {
          nv = new Variable(0, bv->width-1, 0, vcon);
        } else {
          nv = new Variable(0, 0, 0, vcon);
        }
        tsm->AddVar(nv);
      } else if (is_declared(tsm, vcon) == 3) {
        if (bv->isint) {
          nv = new Variable(0, bv->width-1, 0, vcon);
        } else {
          nv = new Variable(0, 0, 0, vcon);
        }
        tsm->AddVar(nv);
      } else if (is_declared(tsm, vcon) == 4) {
				fprintf(stdout, "WHAAAAAAAT?!\n");
				exit(1);
      }

      std::string sid;
      char buf[1024];
      vid->sPrint(buf,1024);
      sid = buf;

      if (!pc) {
        d = new Data (1, bv->width-1, 0, proc, tsm, tmp, tmp, vid, id);
      } else {
        d = new Data (1, bv->width-1, 0, proc, tsm, par_con, pc, vid, id);
      }

      tsm->AddData(sid, d);
    }

    return tmp;

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

							bv->id->toid()->Print(stdout);

							int dir = sub->chpports[j].input;
							int width = bv->width;
							int ischan = bv->ischan;

							Port *ip = new Port(dir,width,ischan,c);
							ip->SetInst();
							ports.push_back(ip);
							iport++;
						}
						smi = new StateMachineInst(p,vx,ar,ports);
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
				}
				sm->AddInst(smi);
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
	fprintf(stdout, "=======================\n");

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
