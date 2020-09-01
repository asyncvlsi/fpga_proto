#include <vector>
#include "state_machine.h"

namespace fpga {

/*
 *  CHP Project Class
 */

void CHPProject::PrintPlain() {
  for (auto n = hd; n; n = n->GetNext()) {
    n->PrintPlain();
  }
}

void CHPProject::Append(StateMachine *sm) {
  if (hd) {
    tl->SetNext(sm);
    tl = sm;
  } else {
    hd = sm;
    hd->SetNext(tl);
    tl = sm;
  }
}


CHPProject::CHPProject() {
  hd = NULL;
  tl = NULL;
}

CHPProject::~CHPProject(){
  StateMachine *cur, *next;
  cur = hd;
  while (cur) {
    next = cur->GetNext();
    delete cur;
    cur = next;
  }
}

/*
 *  State-Machine Class
 */
void StateMachine::AddCondition(Condition *c) {
  if (c->GetType() == 0) {
    commun_num++;
    commu_condition.push_back(c);
  } else if (c->GetType() == 1) {
    guard_num++;
    guard_condition.push_back(c);
  } else if (c->GetType() == 2) {
    st_num++;
    state_condition.push_back(c);
  } else {
    comma_num++;
    comma_condition.push_back(c);
  }
}

void StateMachine::AddSize() {
  size++;
}

void StateMachine::AddKid(StateMachine *sm) {
  csm.push_back(sm);
}

void StateMachine::SetFirstState(State *s) {
  top = s;
  size = 1;
}

int StateMachine::GetNum(){
  return number;
}

int StateMachine::GetSize() {
  return size;
}

int StateMachine::GetKids() {
  return csm.size();
}

int StateMachine::GetGN() { return guard_num; }
int StateMachine::GetSN() { return st_num; }
int StateMachine::GetCN() { return commun_num; }
int StateMachine::GetCCN() { return comma_num; }

void StateMachine::SetNext(StateMachine *smn) {
  next = smn;
}

void StateMachine::SetProcess(Process *p_) {
  p = p_;
}

void StateMachine::SetNumber(int n) {
  number = n;
}

void StateMachine::SetParent(StateMachine *psm) {
  par = psm;
}

bool StateMachine::IsEmpty() {
  if (top) {
    return false;
  } else {
    return true;
  }
}

void StateMachine::PrintState(std::vector<std::pair<State *, Condition *>> s) {
  for (auto ss : s) {
    if (ss.first->isPrinted()) { continue; }
    ss.first->PrintPlain();
  }
  for (auto ss : s) {
    if (ss.first->isPrinted()) { continue; }
    PrintState(ss.first->GetNextState());
  }
}

void StateMachine::PrintPlain() {
  
  top->PrintPlain();
  PrintState(top->GetNextState());

  for (auto c : csm) {
    fprintf(stdout, "===============\n");
    c->PrintPlain();
  }

 // for (auto cc : guard_condition) {
 //   cc->PrintPlain();
 // }
 // for (auto cc : state_condition) {
 //   cc->PrintPlain();
 // }
 // for (auto cc : commu_condition) {
 //   cc->PrintPlain();
 // }
 // for (auto cc : comma_condition) {
 //   cc->PrintPlain();
 // }

}

StateMachine *StateMachine::GetPar() {
  return par;
}

StateMachine *StateMachine::GetNext() {
  return next;
}

StateMachine::StateMachine() {
  p = NULL;
  size = 0;
  number = 0;
  guard_num = 0;
  st_num = 0;
  commun_num = 0;
  comma_num = 0;
  top = NULL;
  next = NULL;
  par = NULL;
}

StateMachine::StateMachine(State *s, int n, Process *p_) {
  p = p_;
  size = 1;
  number = n;
  guard_num = 0;
  st_num = 0;
  commun_num = 0;
  comma_num = 0;
  top = s;
  next = NULL;
  par = NULL;
}

StateMachine::StateMachine(State *s, int n, StateMachine *par_) {
  p = NULL;
  size = 1;
  number = n;
  guard_num = 0;
  st_num = 0;
  commun_num = 0;
  comma_num = 0;
  top = s;
  next = NULL;
  par = par_;
}

StateMachine::~StateMachine() {
  delete top;
  delete p;
}




/*
 *  State Class
 */

void State::PrintPlain() {
  printed = 1;
  if (type == ACT_CHP_SEMI) {
    fprintf(stdout, "SEMI\n");
  } else if (type == ACT_CHP_COMMA) {
    fprintf(stdout, "COMMA\n");
  } else if (type == ACT_CHP_SELECT) {
    fprintf(stdout, "SELECT\n");
  } else if (type == ACT_CHP_SELECT_NONDET) {
    fprintf(stdout, "NONDET\n");
  } else if (type == ACT_CHP_LOOP) {
    fprintf(stdout, "LOOP\n");
  } else if (type == ACT_CHP_SKIP) {
    fprintf(stdout, "SKIP\n");
  } else if (type == ACT_CHP_ASSIGN) {
    fprintf(stdout, "ASSIGN\n");
  } else if (type == ACT_CHP_SEND) {
    fprintf(stdout, "SEND\n");
  } else if (type == ACT_CHP_RECV) {
    fprintf(stdout, "RECV\n");
  } else {
    fprintf(stdout, "Not supported type: %i\n", type);
  }
  for (auto c : ns) {
    fprintf(stdout, "sm%i_state%i->", par->GetNum(), number);
    fprintf(stdout, "sm%i_state%i \n", par->GetNum(), c.first->GetNum());
    fprintf(stdout, " (");
    c.second->PrintPlain();
    fprintf(stdout, " )\n");
  }
}

void State::PrintVerilog() {}

bool State::isPrinted() {
  if (printed == 0) {
    return false;
  } else {
    return true;
  }
}

void State::AddNextState(std::pair<State *, Condition *> s) {
  ns.push_back(s);
}

int State::GetType() {
  return type;
}

int State::GetNum() {
  return number;
}

std::vector<std::pair<State *, Condition *>> State::GetNextState() {
  return ns;
}

State::State(){
  type = 0;
  number = 0;
  printed = 0;
}

State::State(int type_, int number_, StateMachine *par_) {
  type = type_;
  number = number_;
  par = par_;
  printed = 0;
}

State::~State(){}

/*
 *  Condition Class
 */

void Condition::PrintExpr(Expr *e) {
  switch (e->type) {
    case (E_AND):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " & ");
      PrintExpr(e->u.e.r);
      break;
    case (E_OR):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " | ");
      PrintExpr(e->u.e.r);
      break;
    case (E_NOT):
      fprintf(stdout, " ~");
      PrintExpr(e->u.e.l);
      break;
    case (E_PLUS):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " + ");
      PrintExpr(e->u.e.r);
      break;
    case (E_MINUS):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " - ");
      PrintExpr(e->u.e.r);
      break;
    case (E_MULT):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " * ");
      PrintExpr(e->u.e.r);
      break;
    case (E_DIV):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " / ");
      PrintExpr(e->u.e.r);
      break;
    case (E_MOD):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " % ");
      PrintExpr(e->u.e.r);
      break;
    case (E_LSL):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " << ");
      PrintExpr(e->u.e.r);
      break;
    case (E_LSR):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " >> ");
      PrintExpr(e->u.e.r);
      break;
    case (E_ASR):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " >>> ");
      PrintExpr(e->u.e.r);
      break;
    case (E_UMINUS):
      fprintf(stdout, " -");
      PrintExpr(e->u.e.l);
      break;
    case (E_INT):
      fprintf(stdout, "%i", e->u.v);
      break;
    case (E_VAR):
      ActId *id;
      id = (ActId *)e->u.e.l;
      id->Print(stdout);
      fprintf(stdout, " ");
      break;
    case (E_QUERY):
      break;
    case (E_LPAR):
      break;
    case (E_RPAR):
      break;
    case (E_XOR):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " ^ ");
      PrintExpr(e->u.e.r);
      break;
    case (E_LT):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " < ");
      PrintExpr(e->u.e.r);
      break;
    case (E_GT):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " > ");
      PrintExpr(e->u.e.r);
      break;
    case (E_LE):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " <=");
      PrintExpr(e->u.e.r);
      break;
    case (E_GE):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " >= ");
      PrintExpr(e->u.e.r);
      break;
    case (E_EQ):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " == ");
      PrintExpr(e->u.e.r);
      break;
    case (E_NE):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " != ");
      PrintExpr(e->u.e.r);
      break;
    case (E_TRUE):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " 1'b1 ");
      PrintExpr(e->u.e.r);
      break;
    case (E_FALSE):
      PrintExpr(e->u.e.l);
      fprintf(stdout, " 1'b0 ");
      PrintExpr(e->u.e.r);
      break;
    case (E_COLON):
      fprintf(stdout, " : ");
      break;
    case (E_PROBE):
      fprintf(stdout, "PROBE");
      break;
    case (E_COMMA):
      fprintf(stdout, "COMMA");
      break;
    case (E_CONCAT):
      fprintf(stdout, "CONCAT");
      PrintExpr(e->u.e.l);
      fprintf(stdout, " ");
      PrintExpr(e->u.e.r);
      break;
    case (E_BITFIELD):
      fprintf(stdout, "BITFIELD");
      fprintf(stdout, "[");
      PrintExpr(e->u.e.l);
      fprintf(stdout, " ");
      PrintExpr(e->u.e.r);
      fprintf(stdout, "]");
      break;
    case (E_COMPLEMENT):
      fprintf(stdout, " ~");
      PrintExpr(e->u.e.l);
      break;
    case (E_REAL):
      fprintf(stdout, "%i", e->u.v);
      break;
    case (E_RAWFREE):
      break;
    case (E_END):
      break;
    case (E_NUMBER):
      break;
    case (E_FUNCTION):
      break;
    default:
      fprintf(stdout, "What?! %i\n", e->type);
      break;
  }

}

int Condition::GetType(){
  return type;
}

int Condition::GetNum() {
  return num;
}

void Condition::PrintPlain() {
  fprintf(stdout, "Condition number : %i\n", num);
  switch (type) {
    case (0) :
      fprintf(stdout, "communication_completion = ");
      u.v->Print(stdout);
      fprintf(stdout, "_valid & ");
      u.v->Print(stdout);
      fprintf(stdout, "_ready");
      break;
    case (1) :
      fprintf(stdout, "guard = ");
      PrintExpr(u.e);
      break;
    case (2) :
      fprintf(stdout, "state = state%i", u.st_num);
      break;
    case (3) :
      fprintf(stdout, "cond = ");
      for (auto cc : u.c->c) {
        if (cc->GetType() == 0) {
          fprintf(stdout, "commun_compl%i ", cc->GetNum());
        } else if (cc->GetType() == 1) {
          fprintf(stdout, "guard%i ", cc->GetNum());
        } else if (cc->GetType() == 2) {
          fprintf(stdout, "state%i ", cc->GetNum());
        } else {
          fprintf(stdout, "comma%i ", cc->GetNum());
        }
        if (cc != u.c->c[u.c->c.size()-1]) {
          if (u.c->type != 1) {
            fprintf(stdout," & ");
          } else {
            fprintf(stdout," | ");
          }
        }
      }
      break;
  }
  fprintf(stdout,"\n");
}

void Condition::PrintVerilog(){}

Condition::Condition() {
  type = 0;
  num = 0;
  u.v = NULL;
}

Condition::Condition(ActId *v_, int num_) {
  type = 0;
  num = num_;
  u.v = v_;
}

Condition::Condition(Expr *e_, int num_) {
  type = 1;
  num = num_;
  u.e = e_;
}

Condition::Condition(int st_num_, int num_) {
  type = 2;
  num = num_;
  u.st_num = st_num_;
}

Condition::Condition(Comma *c_, int num_) {
  type = 3;
  num = num_;
  u.c = c_;
}


}
