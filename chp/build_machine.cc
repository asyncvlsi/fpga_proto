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
//or not. True - declared, False - not declared
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
  
  return 0;
}

//Function to walk through expression tree and collect
//all variables for later analisys
void collect_vars(Expr *e, std::vector<ActId *> &vars) {
  if (e->type == E_VAR) {
    ActId *id = (ActId *)e->u.e.l;
    vars.push_back(id);
  } else if (e->type == E_INT ||
             e->type == E_REAL) {
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
                        State *cs,          //current state
                        State *ns,          //next state
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

    State *s = NULL;
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_COMMA, sm->GetSize(), sm);
      sm->SetFirstState(s);
    }

    Condition *child_cond;
    if (s) {
      child_cond = new Condition(s, sm->GetSN(), sm);
    } else {
      child_cond = pc;
    }

    for (li = list_first(l); li; li = list_next(li)) {

      cl = (act_chp_lang_t *)list_value(li);

      if (cl->type == ACT_CHP_SKIP) { continue; }

      if (is_simple(cl->type)) {
        if (s) {
          tmp = traverse_chp(proc, cl, sm, tsm, s, s, child_cond);
        } else {
          tmp = traverse_chp(proc, cl, sm, tsm, NULL, s, child_cond);
        }
      } else {
        StateMachine *csm = new StateMachine();
        csm->SetNumber(sm->GetKids());
        csm->SetParent(sm);
        sm->AddKid(csm);
        tmp = traverse_chp(proc, cl, csm, tsm , NULL, NULL, child_cond);
      }

      if (tmp) {
        sm->AddCondition(tmp);
        vc.push_back(tmp);
      }

    }
    if (s) {
      sm->AddCondition(child_cond);
      vc.push_back(child_cond);
    }

    Comma *com = new Comma;
    com->type = 0;
    com->c = vc;
    Condition *term = new Condition(com, sm->GetCCN(), sm);


    if (!sm->GetPar()) {
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
    //between two semicolons. If statement is simple then 
    //exaluation takes one cycle and switches to the next
    //state. If it is complex then new state machine is created
    //and building process initiated.

    l = chp_lang->u.semi_comma.cmd;

    State *s = NULL;
    State *first_s = NULL;
    State *prev_s = NULL;
    Condition *prev_c = NULL;
    Condition *child_cond = NULL;
    Condition *next_con = NULL;

    for (li = list_first(l); li; li = list_next(li)) {

      cl = (act_chp_lang_t *)list_value(li);

      if (cl->type != ACT_CHP_SKIP) {

        s = new State(cl->type, sm->GetSize(), sm);
        
        child_cond = new Condition(s, sm->GetSN(), sm);
        sm->AddCondition(child_cond);

        if (sm->IsEmpty()) {
          sm->SetFirstState(s);
        } else {
          sm->AddSize();
        }
        
        if (li == list_first(l)) {
          first_s = s;
        }

        if (is_simple(cl->type)) {
          tmp = traverse_chp(proc, cl, sm, tsm, s, s, child_cond);
          sm->AddCondition(tmp);
        } else {
          StateMachine *csm = new StateMachine();
          csm->SetNumber(sm->GetKids());
          csm->SetParent(sm);
          sm->AddKid(csm);
          tmp = traverse_chp(proc, cl, csm, tsm, NULL, NULL, child_cond);
        }

        std::pair<State *, Condition *> n;
      
        if (prev_s) {
          n.first = s;
          n.second = prev_c;
          prev_s->AddNextState(n);
        }
   
        Comma *next_com = new Comma;
        next_com->type = 0;
        next_com->c.push_back(tmp);
        next_com->c.push_back(child_cond);
        if (pc) {
          next_com->c.push_back(pc);
        }
        
        next_con = new Condition(next_com, sm->GetCCN(), sm);

        prev_s = s;
        prev_c = next_con;

        sm->AddCondition(next_con);
      }

    }

    Comma *com = new Comma;
    com->type = 0;
    com->c.push_back(tmp);
    com->c.push_back(child_cond);
    Condition *term = new Condition(com, sm->GetCCN(), sm);


    if (sm->GetPar()) {
      n.first = first_s;
      n.second = next_con;
      s->AddNextState(n);
      return next_con;
    } else if (!sm->GetPar()) {
      State *ds = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
      n.first = ds;
      n.second = tmp;
      s->AddNextState(n);
      return tmp;
    } 

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
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_SELECT, 0, sm);
      sm->SetFirstState(s);
    }

    //walk through all selection options
    std::pair<State *, Condition *> n;
    State *ss = NULL;

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {
      if (gg->g) {
        if (gg->s && gg->s->type != ACT_CHP_SKIP) {
          Condition *g = new Condition(gg->g, sm->GetGN(), sm);
          sm->AddCondition(g);
          n.second = g;
        }
      }
      if (gg->s) {
        if (gg->s->type != ACT_CHP_SKIP) {
          ss = new State(gg->s->type, sm->GetSize(), sm);
          n.first = ss;
          sm->AddSize();
          s->AddNextState(n);
          if (is_simple(gg->s->type)) {
            tmp = traverse_chp(proc, gg->s, sm, tsm, ss, s, NULL);
          } else {
            StateMachine *csm = new StateMachine();
            csm->SetNumber(sm->GetKids());
            csm->SetParent(sm);
            sm->AddKid(csm);
            tmp = traverse_chp(proc, gg->s, csm, tsm, NULL, NULL, NULL);
          }
          if (tmp) {
            vc.push_back(tmp);
          }
        }
      }
    }

    Comma *com = new Comma;
    com->type = 1;
    com->c = vc;

    Condition *term = new Condition(com, sm->GetCCN(), sm);

    if (!ns && !sm->GetPar()) {
      ss = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
      n.first = ss;
      n.second = term;
      s->AddNextState(n);
    }

    sm->AddCondition(term);

    return term;

    break;
  }
  case ACT_CHP_SELECT_NONDET: {

    fprintf(stdout, "//NONDET\n");

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {
      Condition *g = new Condition(gg->g, sm->GetGN(), sm);
      sm->AddCondition(g);
      traverse_chp (proc, chp_lang->u.gc->s, sm, tsm, NULL, NULL, NULL);
    }
    break;
  }
  case ACT_CHP_LOOP: {

    fprintf(stdout, "//LOOP\n");
    //Loop type states keep executing while at least one guard
    //stays true. Termination condition is AND of all all guards
    //with negation.

    int inf_flag = 0;

    State *s = NULL;
    s = new State(ACT_CHP_LOOP, 0, sm);
    sm->SetFirstState(s);

    Condition *child_cond = NULL;

    Condition *zero_state_cond = new Condition(s, sm->GetSN(), sm);
    sm->AddCondition(zero_state_cond);

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {

      std::pair<State *, Condition *> n;

      Condition *g = NULL;
      State *ss = NULL;
      if (gg->g) {
        if (gg->s->type != ACT_CHP_SKIP) {
          g = new Condition(gg->g, sm->GetGN(), sm);
          sm->AddCondition(g);
        }
        vt.push_back(g);
      } else {
        inf_flag = 1; 
        vt.push_back(zero_state_cond);
      }

      if (gg->s) {
        if (gg->s->type != ACT_CHP_SKIP) {
          ss = new State(gg->s->type, sm->GetSize(), sm);
          sm->AddSize();
          n.first = ss;
          if (!pc) {
            if (g) {
              n.second = g;
            } else {
              n.second = zero_state_cond;
            }
          } else {
            Comma *par_com = new Comma();
            par_com->type = 0;
            if (g) {
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

          child_cond = new Condition(ss, sm->GetSN(), sm);

          if (is_simple(gg->s->type)) {
            tmp = traverse_chp(proc, gg->s, sm, tsm, ss, s, child_cond);
          } else {
            StateMachine *csm = new StateMachine();
            csm->SetNumber(sm->GetKids());
            csm->SetParent(sm);
            sm->AddKid(csm);
            tmp = traverse_chp(proc, gg->s, csm, tsm, NULL, NULL, child_cond);
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
          sm->AddCondition(child_cond);
        }
      }
    }

    Comma *no_g_com = new Comma;
    no_g_com->type = 2;
    no_g_com->c = vt;
    Condition *no_guard = new Condition(no_g_com, sm->GetCCN(), sm);
    sm->AddCondition(no_guard);

    Comma *term_com = new Comma();
    term_com->c.push_back(zero_state_cond);
    term_com->c.push_back(no_guard);
    term_com->type = 0;
    Condition *term_cond = new Condition(term_com, sm->GetCCN(), sm);

    if (!ns && !pc && inf_flag == 0) {
      State *ds = new State(ACT_CHP_SKIP, sm->GetSize(), sm);
      n.first = ds;
      n.second = term_cond;
      s->AddNextState(n);
    } else if (pc) {
      n.first = s;
      n.second = term_cond;
      s->AddNextState(n);
    }

    if (inf_flag == 0) {
      sm->AddCondition(term_cond);
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
    } else {
      tmp = new Condition(cs, sm->GetSN(), sm);
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
      nv = new Variable(0, 0, idc);
      tsm->AddVar(nv);
    }

    std::vector<ActId *> var_col;
    collect_vars(e, var_col);
    for (auto v : var_col) {
      act_connection *cur_con = v->Canonical(scope);
      hb = ihash_lookup(bnl->cH, (long)cur_con);
      bv = (act_booleanized_var_t *)hb->v;

      if (is_declared(tsm, cur_con) == 0) {
        if (bv->ischan == 0 && bv->isint == 0) {
          nv = new Variable(0,0,cur_con);
        } else {
          nv = new Variable(0,bv->width-1,cur_con);
        }
        tsm->AddVar(nv);
      } else if (is_declared(tsm, cur_con) == 4) {
        if (bv->ischan == 0 && bv->isint == 0) {
          nv = new Variable(1,0,cur_con);
        } else {
          nv = new Variable(1,bv->width-1,cur_con);
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
      tsm->AddData(wc-1,0, sid, d);
      return tmp;
    } else if (pc) {
      d = new Data(0, wc-1, 0, proc, tsm, pc, NULL, id, e);
      tsm->AddData(wc-1, 0, sid, d);
      return NULL;
    } else {
      fprintf(stdout, "Very interesting...\n");
    }

    break;
  }
  case ACT_CHP_SEND: {

    fprintf(stdout, "//SEND\n");

    //adding communacation completion condition
    //TODO: send can be more complicated
    State *s = NULL;
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_SEND, 0, sm);
      sm->SetFirstState(s);
    }

    ActId *id;
    id = chp_lang->u.comm.chan;
    tmp = new Condition(id, sm->GetCN(), sm);

    std::pair<State *, Condition *> n;
    Condition *par_con = NULL;


    if (!pc) {
      n.first = s;
      n.second = tmp;
      s->AddNextState(n);
    } else if (pc) {
      Comma *par_com = new Comma;
      par_com->type = 0;
      par_com->c.push_back(tmp);
      par_com->c.push_back(pc);
      par_con = new Condition(par_com, sm->GetCCN(), sm);
    } else {
      fprintf(stdout, "Very interesting...\n");
    }

    Data *d = NULL;
    act_connection *ccon;
    ccon = id->Canonical(scope);
    std::string sid;
    char buf[1024];
    id->sPrint(buf,1024);
    sid = buf;

    int wc = 0;
    int up = 0;
    int dn = 0;

    std::vector<ActId *> var_col; //collection of variables from
                                  //the send expression

    ihash_bucket_t *hb;
    act_booleanized_var_t *bv;
    hb = ihash_lookup(bnl->cH, (long)ccon);
    bv = (act_booleanized_var_t *)hb->v;
    wc = bv->width;

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
            nv = new Variable(0, bv->width-1, cur_con);
          } else {
            nv = new Variable(0, 0, cur_con);
          }
          tsm->AddVar(nv);
        } else if (is_declared(tsm, cur_con) == 4) {
          if (bv->isint == 1 || bv->ischan == 1) {
            Variable *nv = new Variable(1, bv->width-1, cur_con);
          } else {
            Variable *nv = new Variable(1, 0, cur_con);
          }
          tsm->AddVar(nv);
        }
      }

      ActId *main_var;
      main_var = var_col[0];
      act_connection *main_con;
      main_con = main_var->Canonical(scope);
      hb = ihash_lookup(bnl->cH, (long)main_con);
      bv = (act_booleanized_var_t *) hb->v;

      if (bv->usedchp) {
        if (dn == 0) {
          dn = up;
        }
        up = up + bv->width;
      } else {
        continue;
      }

      if (!pc) {
        d = new Data (2, up-1, dn, proc, tsm, tmp, tmp, id, vex);
      } else if (pc) {
        d = new Data (2, up-1, dn, proc, tsm, par_con, pc, id, vex);
      } else {
        fprintf(stdout, "Wow!\n");
      }
   
      tsm->AddData(up-1, dn, sid, d);

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
    id = chp_lang->u.comm.chan;
    tmp = new Condition(id, sm->GetCN(), sm);

    std::pair<State *, Condition *> n;
    Condition *par_con = NULL;

    if (!pc) {
      n.first = s;
      n.second = tmp;
      s->AddNextState(n);
    } else if (pc) {
      Comma *par_com = new Comma;
      par_com->type = 0;
      par_com->c.push_back(tmp);
      par_com->c.push_back(pc);
      par_con = new Condition(par_com, sm->GetCCN(), sm);
    } else {
      fprintf(stdout, "Very interesting...\n");
    }

    Data *d = NULL;
    act_connection *ccon;         //channel connection
    ccon = id->Canonical(scope);  //caconical connection
    int wc = 0;                   //channel total width
    int up = 0;                   //channel slice upper boarder
    int dn = 0;                   //channel slice lower boarder
    ihash_bucket_t *hb;
    act_booleanized_var_t *bv; //booleanized channel
    hb = ihash_lookup(bnl->cH, (long)ccon);
    bv = (act_booleanized_var_t *)hb->v;
    wc = bv->width;

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
          nv = new Variable(0, bv->width-1, vcon);
        } else {
          nv = new Variable(0, 0, vcon);
        }
        tsm->AddVar(nv);
      } else if (is_declared(tsm, vcon) == 4) {
        if (bv->isint) {
          nv = new Variable(1, bv->width-1, vcon);
        } else {
          nv = new Variable(1, 0, vcon);
        }
        tsm->AddVar(nv);
      }

      if (bv->usedchp) {
        if (dn == 0) {
          dn = up;
        }
        up = up + bv->width;
      } else {
        fprintf(stdout, "WARNING: How is this even possible!\n");
      }

      if (up > wc) {
        fprintf(stdout, "WARNING: Variable is out of range!\n");
      }

      std::string sid;
      char buf[1024];
      vid->sPrint(buf,1024);
      sid = buf;

      if (!pc) {
        d = new Data (1, up-1, dn, proc, tsm, tmp, tmp, vid, id);
      } else if (pc) {
        d = new Data (1, up-1, dn, proc, tsm, par_con, pc, vid, id);
      } else {
        fprintf(stdout, "Wow!\n");
      }

      tsm->AddData(bv->width-1, 0, sid, d);

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

  act_chp_lang_t *chp_lang = chp->c;

  //Create state machine for the currect
  //process and with the initial state0 
  StateMachine *sm = new StateMachine();
  sm->SetProcess(p);
  sm->SetNumber(0);

  //add ports
  add_ports(cs, bnl, sm);

  //run chp traverse to build state machine
  traverse_chp(p, chp_lang, sm, sm, NULL, NULL, NULL);

  //append linked list of chp project
  //processes
  cp->Append(sm);
}

CHPProject *build_machine (Act *a, Process *p) {

  ActPass *apb = a->pass_find("booleanize");

  BOOL = dynamic_cast<ActBooleanizePass *>(apb);

  CHPProject *cp = new CHPProject();
 
  traverse_act (p, cp);

  return cp;
}

}
