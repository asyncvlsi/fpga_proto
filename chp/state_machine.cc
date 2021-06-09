#include <vector>
#include <act/state_machine.h>

namespace fpga {

/*
 *  CHP Project Class
 */

StateMachine *CHPProject::Head() {
	return hd;
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
bool StateMachine::IsEmpty() {
  if (top) {
    return false;
  } else {
    return true;
  }
}

int StateMachine::IsPort(act_connection *c_) {
  int port_type = 0;
  for (auto p : ports) {
    if (p->GetCon() == c_) {
      if (p->GetDir() == 0) {
        return 1;
      } else {
        return 2;
      }
    }
  }
  for (auto in : inst) {
		for (auto ip : in->GetPorts()) {
			if (ip->GetCon() == c_){
				if (ip->GetDir() == 0) {
					return 2;
				} else {
					return 1;
				}
			}
		}
	}
  return 0;
}

void StateMachine::AddInst (StateMachineInst *inst_) {
	inst.push_back(inst_);
}
void StateMachine::AddSize() { size++; }
void StateMachine::AddKid(StateMachine *sm) { csm.push_back(sm); }
void StateMachine::AddPort(Port *p_){ ports.push_back(p_); }
void StateMachine::AddVar(Variable *v_){ 
	vars.push_back(v_); 
	vm[v_->GetId()] = v_;
}
void StateMachine::AddData(ValueIdx* id, Data *dd) {data[id].push_back(dd);}
void StateMachine::AddHS(ValueIdx* id, Data *dd) {hs_data[id].push_back(dd);}
void StateMachine::AddCondition(Condition *c) {
  if (c->GetType() == 0) {
    commun_num++;
    commu_condition.push_back(c);
  } else if (c->GetType() == 1 || c->GetType() == 4) {
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
void StateMachine::AddArb(Arbiter *a) { arb.push_back(a); }

int StateMachine::GetNum(){ return number; }
int StateMachine::GetSize() { return size; }
int StateMachine::GetKids() { return csm.size(); }
int StateMachine::GetGN() { return guard_num; }
int StateMachine::GetSN() { return st_num; }
int StateMachine::GetCN() { return commun_num; }
int StateMachine::GetCCN() { return comma_num; }
Variable *StateMachine::GetVar(ValueIdx *vx) { return vm[vx]; }
std::vector<Variable *> StateMachine::GetVars(){ return vars; }
std::vector<Port *> StateMachine::GetPorts(){ return ports; }
StateMachine *StateMachine::GetPar() { return par; }
StateMachine *StateMachine::GetNext() { return next; }
Process *StateMachine::GetProc() { return p; }
std::vector<StateMachineInst *> StateMachine::GetInst() { return inst; }

void StateMachine::SetNext(StateMachine *smn) { next = smn; }
void StateMachine::SetProcess(Process *p_) { p = p_; }
void StateMachine::SetNumber(int n) { number = n; }
void StateMachine::SetParent(StateMachine *psm) { par = psm; }
void StateMachine::SetFirstState(State *s) {
  top = s;
  size = 1;
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
 *	State Machine Instance Class
 */
void StateMachineInst::SetSM(StateMachine *sm_){
	sm = sm_;
}

void StateMachineInst::SetCtrlChan(int i) {
	ports[i]->SetCtrlChan();
}

Process *StateMachineInst::GetProc(){
	return p;
}

std::vector<Port *> StateMachineInst::GetPorts(){
	return ports;
}

StateMachineInst::StateMachineInst(){
	p = NULL;
	name = NULL;
	array = NULL;
	sm = NULL;
};

StateMachineInst::StateMachineInst(
																	Process *p_,
																	ValueIdx *name_,
																	char *array_,
																	std::vector<Port *> &ports_
																	){
	p = p_;
	name = name_;
	array = array_;
	ports = ports_;
	sm = NULL;

}

StateMachineInst::~StateMachineInst(){};

/*
 *  State Class
 */

bool State::isPrinted() {
  if (printed == 0) {
    return false;
  } else {
    return true;
  }
}

void State::AddNextState(std::pair<State *, Condition *> s) { ns.push_back(s); }

int State::GetType() { return type; }
int State::GetNum() { return number; }
StateMachine *State::GetPar() { return par; }
std::vector<std::pair<State *, Condition *>> State::GetNextState() { return ns; }

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

int Condition::GetType(){ return type; }
int Condition::GetNum() { return num; }
State *Condition::GetState() { return u.s; }
StateMachine *Condition::GetScope() { return scope; }

Condition::Condition() {
  type = 0;
  num = 0;
  u.v = NULL;
  scope = NULL;
}

Condition::Condition(ActId *v_, int num_, StateMachine *sc) {
  type = 0;
  num = num_;
  u.v = v_;
  scope = sc;
}

Condition::Condition(Expr *e_, int num_, StateMachine *sc) {
  type = 1;
  num = num_;
  u.e = e_;
  scope = sc;
}

Condition::Condition(State *s_, int num_, StateMachine *sc) {
  type = 2;
  num = num_;
  u.s = s_;
  scope = sc;
}

Condition::Condition(Comma *c_, int num_, StateMachine *sc) {
  type = 3;
  num = num_;
  u.c = c_;
  scope = sc;
}

Condition::Condition(Expr *e_, int num_, StateMachine *sc, int a) {
  type = 4;
  num = num_;
  u.e = e_;
  scope = sc;
}

/*
 *	Arbiter Class
 */
void Arbiter::AddElement(Condition *c) {
	a.push_back(c);
}

Arbiter::Arbiter(){}
Arbiter::~Arbiter(){}

}
