#include "act/state_machine.h"
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
    return false;
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

//Function to traverse CHP description and collect all 
//neccessary data to to build state machine. Function 
//returns condition type to handle cases where return 
//state might be one of the previously added states.
//For exmaple, loops or commas.
Condition *traverse_chp (act_chp_lang_t *chp_lang, 
                         StateMachine *sm,
                         State *cs = NULL,  //current state
                         State *rs = NULL   //return state
                        ) {
    
  std::vector<Condition *> vc;
  list_t *l;
  listitem_t *li;
  act_chp_lang_t *cl;

  Condition *tmp;

  switch (chp_lang->type) {

  case ACT_CHP_COMMA: {

    fprintf(stdout, "COMMA\n");

    //Comma type completions means concurrent completion of 
    //all commaed statements. If statement is simple then 
    //its evaluation happens at the current state of the 
    //current state machine. If Statement is complex, then 
    //new state machine is created and new top state as well.
    //For select and loop new state is the state where all
    //guards are evaluatied.

    l = chp_lang->u.semi_comma.cmd;

    State *s = NULL;
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_COMMA, sm->GetSize(), sm);
      sm->SetFirstState(s);
    }

    for (li = list_first(l); li; li = list_next(li)) {

      cl = (act_chp_lang_t *)list_value(li);

      if (is_simple(cl->type)) {
        if (s) {
          tmp = traverse_chp(cl, sm, s, s);
        } else {
          tmp = traverse_chp(cl, sm, s, cs);
        }
      } else {
        StateMachine *csm = new StateMachine();
        csm->SetNumber(sm->GetKids()+1);
        csm->SetParent(sm);
        sm->AddKid(csm);
        tmp = traverse_chp(cl, csm, NULL, NULL);
      }

      sm->AddCondition(tmp);

      vc.push_back(tmp);

    }

    Comma *com = new Comma;
    com->type = 0;
    com->c = vc;
    Condition *term = new Condition(com, sm->GetCCN());

    sm->AddCondition(term);

    return term;

    break;
  }
  case ACT_CHP_SEMI: {

    fprintf(stdout, "SEMI\n");

    //Semi type completion means completion of the statement
    //between two semicolons. If statement is simple then 
    //exaluation takes one cycle and switches to the next
    //state. If it is complex then new state machine is created
    //and bulding process initiated.

    l = chp_lang->u.semi_comma.cmd;

    State *first_s = NULL;
    State *prev_s = NULL;
    Condition *prev_c = NULL;

    for (li = list_first(l); li; li = list_next(li)) {

      cl = (act_chp_lang_t *)list_value(li);

      State *s = new State(cl->type, sm->GetSize(), sm);

      if (sm->IsEmpty()) {
        sm->SetFirstState(s);
      } else {
        sm->AddSize();
      }

      if (li == list_first(l)) {
        first_s = s;
      }

      if (is_simple(cl->type)) {
        tmp = traverse_chp(cl, sm, s, cs);
      } else {
        StateMachine *csm = new StateMachine();
        csm->SetNumber(sm->GetKids()+1);
        csm->SetParent(sm);
        sm->AddKid(csm);
        tmp = traverse_chp(cl, csm, NULL, NULL);
      }
    
      sm->AddCondition(tmp);

      std::pair<State *, Condition *> n;

      if (prev_s) {
        fprintf(stdout, "HERE\n");
        n.first = s;
        n.second = prev_c;
        prev_s->AddNextState(n);
      }

      if (!list_next(li)) {
        fprintf(stdout, "THERE\n");
        n.first = first_s;
        n.second = tmp;
        s->AddNextState(n);
      } 

      prev_s = s;
      prev_c = tmp;

    }

    return tmp;

    break;
  }
  case ACT_CHP_SELECT: {

    fprintf(stdout, "SELECT\n");
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

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {
      if (gg->g) {
        Condition *g = new Condition(gg->g, sm->GetGN());
        sm->AddCondition(g);
        n.second = g;
      }
      if (gg->s) {
        State *ss = new State(gg->s->type, sm->GetSize(), sm);
        n.first = ss;
        sm->AddSize();
        if (s) {
          s->AddNextState(n);
        } else {
          cs->AddNextState(n);
        }
        if (is_simple(gg->s->type)) {
          if (s) {
            tmp = traverse_chp(gg->s, sm, ss, s);
          } else {
            tmp = traverse_chp(gg->s, sm, ss, cs);
          }
        } else {
          StateMachine *csm = new StateMachine();
          csm->SetNumber(sm->GetKids()+1);
          csm->SetParent(sm);
          sm->AddKid(csm);
          tmp = traverse_chp(gg->s, csm, NULL, NULL);
        }
      }
      vc.push_back(tmp);
    }

    Comma *com = new Comma;
    com->type = 1;
    com->c = vc;

    Condition *term = new Condition(com, sm->GetCCN());

    //if (s) {
    //  n.first = s;  
    //  n.second = term;
    //  s->AddNextState(n);
    //} else {
    //  n.first = cs;  
    //  n.second = term;
    //  cs->AddNextState(n);
    //}

    sm->AddCondition(term);
    return term;

    break;
  }
  case ACT_CHP_SELECT_NONDET: {

    fprintf(stdout, "NONDET\n");

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {
      Condition *g = new Condition(gg->g, sm->GetGN());
      sm->AddCondition(g);
      traverse_chp (chp_lang->u.gc->s, sm, rs);
    }
    break;
  }
  case ACT_CHP_LOOP: {

    fprintf(stdout, "LOOP\n");
    //Loop type states keep executing while at least one guard
    //stays true. Termination condition is AND of all all guards
    //with negation.

    State *s = NULL;
    if (!cs) {
      s = new State(ACT_CHP_LOOP, 0, sm);
      sm->SetFirstState(s);
    }

    for (auto gg = chp_lang->u.gc; gg; gg = gg->next) {

      std::pair<State *, Condition *> n;

      if (gg->g) {
        Condition *g = new Condition(gg->g, sm->GetGN());
        sm->AddCondition(g);
        vc.push_back(g);
        n.second = g;
      }

      if (gg->s) {
        State *ss = new State(gg->s->type, sm->GetSize(), sm);
        sm->AddSize();
        n.first = ss;
        if (s) {
          s->AddNextState(n);
        } else {
          cs->AddNextState(n);
        }
        if (is_simple(gg->s->type)) {
          if (s) {
            tmp = traverse_chp(gg->s, sm, ss, s);
          } else {
            tmp = traverse_chp(gg->s, sm, ss, cs);
          }
        } else {
          StateMachine *csm = new StateMachine();
          csm->SetNumber(sm->GetKids()+1);
          csm->SetParent(sm);
          sm->AddKid(csm);
          tmp = traverse_chp(gg->s, csm, NULL, NULL);
        }
        if (s) {
          n.first = s;
          n.second = tmp;
          ss->AddNextState(n);
        } else {
          n.first = cs;
          n.second = tmp;
          ss->AddNextState(n);
        }
      }
    }

    Comma *com = new Comma;
    com->type = 2;
    com->c = vc;
    Condition *term = new Condition(com, sm->GetCCN());

    sm->AddCondition(term);
    return term;

    break;
  }
  case ACT_CHP_DOLOOP: {
    fprintf(stdout, "DOLOOP\n");
    break;
  }
  case ACT_CHP_SKIP: {
    fprintf(stdout, "SKIP\n");
    break;
  }
  case ACT_CHP_ASSIGN: {

    fprintf(stdout, "ASSIGN\n");

    //adding state condition, since assignment
    //takes only one cycle.
    State *s = NULL;
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_ASSIGN, 0, sm);
      sm->SetFirstState(s);
    }

    Condition *c = new Condition(sm->GetSize()-1, sm->GetSN());
    sm->AddCondition(c);

    std::pair<State *, Condition *> n;

    if (s) {
      n.first = s;
      n.second = c;
      s->AddNextState(n);
    } else if (rs) {
      n.first = rs;
      n.second = c;
      cs->AddNextState(n);
    }

    return c;

    break;
  }
  case ACT_CHP_SEND: {

    fprintf(stdout, "SEND\n");

    //adding communacation completion condition
    //TODO: send can be more complicated
    State *s = NULL;
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_SEND, 0, sm);
      sm->SetFirstState(s);
    }

    ActId *id;
    id = chp_lang->u.comm.chan;
    Condition *c = new Condition(id, sm->GetCN());
    sm->AddCondition(c);

    std::pair<State *, Condition *> n;

    if (s) {
      n.first = s;
      n.second = c;
      s->AddNextState(n);
    } else if (rs) {
      n.first = rs;
      n.second = c;
      cs->AddNextState(n);
    }

    return c;

    break;
  }
  case ACT_CHP_RECV: {

    fprintf(stdout, "RECV\n");

    //adding communacation completion condition
    //TODO: recv of a list of vars is not handeled
    State *s = NULL;
    if (sm->IsEmpty()) {
      s = new State(ACT_CHP_RECV, 0, sm);
      sm->SetFirstState(s);
    }


    ActId *id;
    id = chp_lang->u.comm.chan;
    Condition *c = new Condition(id, sm->GetCN());
    sm->AddCondition(c);

    std::pair<State *, Condition *> n;

    if (s) {
      n.first = s;
      n.second = c;
      s->AddNextState(n);
    } else if (rs) {
      n.first = rs;
      n.second = c;
      cs->AddNextState(n);
    }

    return c;

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

  //run chp traverse to build state machine
  traverse_chp(chp_lang, sm);

//  sm->PrintPlain();

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
