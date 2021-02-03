#include <act/state_machine.h>

namespace fpga {

/*
 *  Data class
 */

int Data::GetType() {
  return type;
}

Condition *Data::GetCond() {
  return cond;
}

Process *Data::GetActScope() {
  return act_scope;
}

StateMachine *Data::GetScope() {
  return scope;
}

ActId *Data::GetId() {
  return id;
}

Data::Data() {
  cond = 0;
  up = 0;
  dn = 0;
  type = 0;
  id = NULL;
  u.assign.e = NULL;
  scope = NULL;
  act_scope = NULL;
  printed = 0;
}

Data::Data (int type_, 
            int up_,
            int dn_,
            Process *act_scope_, 
            StateMachine *scope_, 
            Condition *cond_, 
            Condition *up_cond_, 
            ActId *id_, 
            Expr *e_) {
  cond = cond_;
  type = type_;
  up = up_;
  dn = dn_;
  id = id_;
  if (type_ == 0) {
    u.assign.e = e_;
  } else if (type == 2) {
    u.send.up_cond = up_cond_;
    u.send.se = e_;
  }
  scope = scope_;
  act_scope = act_scope_;
  printed = 0;
}

Data::Data (int type_,
            int up_,
            int dn_,
            Process *act_scope_, 
            StateMachine *scope_,
            Condition *cond_, 
            Condition *up_cond_, 
            ActId *id_, 
            ActId *rhs_) {
  cond = cond_;
  type = type_;
  up = up_;
  dn = dn_;
  id = id_;
  u.recv.up_cond = up_cond_;
  u.recv.chan = rhs_;
  scope = scope_;
  act_scope = act_scope_;
  printed = 0;
}

Data::~Data () {}

/*
 *  Port Class
 */
int Port::GetChan() {
	return ischan;
}

int Port::GetDir() {
  return dir;
}

act_connection *Port::GetCon(){
  return connection;
}

int Port::GetInst() {
	return inst;
}

void Port::SetInst() {
	inst = 1;
}

Port::Port(){
  dir = 0;
  width = 0;
  ischan = 0;
	inst = 0;
  connection = NULL;
}

Port::Port(int dir_, int width_, int chan_, act_connection *c_){
  dir = dir_;
  width = width_;
  ischan = chan_;
	inst = 0;
  connection = c_;
}

Port::~Port() {}

/*
 *  Variable Class
 */
ValueIdx *Variable::GetId(){
	return vx;
}

int Variable::GetDimNum(){
	return dim.size();
}

void Variable::AddDimension(int d) {
	dim.push_back(d);
}

int Variable::IsChan() {
	return ischan;
}

Variable::Variable() {
  type = 0;
	ischan = 0;
	vx = NULL;
}

Variable::Variable(int type_, int ischan_) {
  type = type_;
	ischan = ischan_;
	vx = NULL;
}

Variable::Variable(int type_, int ischan_, ValueIdx *vx_) {
  type = type_;
	ischan = ischan_;
	vx = vx_;
}

}
