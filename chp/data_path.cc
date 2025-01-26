#include "state_machine.h"

namespace fpga {

/*
 *  Data class
 */

int CHPData::GetType() {
  return type;
}

Condition *CHPData::GetCond() {
  return cond;
}

Process *CHPData::GetActScope() {
  return act_scope;
}

StateMachine *CHPData::GetScope() {
  return scope;
}

ActId *CHPData::GetId() {
  return id;
}

CHPData::CHPData() {
  cond = 0;
  type = 0;
  id = NULL;
  u.assign.e = NULL;
  scope = NULL;
  act_scope = NULL;
  printed = 0;
}

CHPData::CHPData (int type_, 
            Process *act_scope_, 
            StateMachine *scope_, 
            Condition *cond_, 
            Condition *up_cond_, 
            ActId *id_, 
            Expr *e_) {
  cond = cond_;
  type = type_;
  id = id_;
  if (type_ == 0) {
    u.assign.e = e_;
  } else if (type == 2 || type == 5) {
    u.send.up_cond = up_cond_;
    u.send.se = e_;
  }
  scope = scope_;
  act_scope = act_scope_;
  printed = 0;
}

CHPData::CHPData (int type_,
            Process *act_scope_, 
            StateMachine *scope_,
            Condition *cond_, 
            Condition *up_cond_, 
            ActId *id_, 
            ActId *rhs_) {
  cond = cond_;
  type = type_;
  id = id_;
  if (type == 7) {
    u.send_struct.up_cond = up_cond_;
    u.send_struct.chan = rhs_;
  } else {
    u.recv.up_cond = up_cond_;
    u.recv.chan = rhs_;
  }
  scope = scope_;
  act_scope = act_scope_;
  printed = 0;
}

CHPData::~CHPData () {}

/*
 *  Port Class
 */
int Port::GetChan() {	return ischan;}
int Port::GetDir() {  return dir;}
act_connection *Port::GetCon(){  return connection;}
ValueIdx *Port::GetVx() { return root_id; } 
int Port::GetInst() { return inst; }

void Port::SetInst() {	inst = 1;}
void Port::SetCtrlChan() {
	if (ischan == 0) {
		fatal_error("can't use it");
	} else {
		ischan = 2;
	}
}

Port::Port(){
  dir = 0;
  width = 0;
  ischan = 0;
	inst = 0;
	reg = 0;
	root_id = NULL;
  connection = NULL;
  o_type = 0;
  owner.sm = NULL;
  owner.smi = NULL;
}

Port::Port(Port *p){
  dir = p->dir;
  width = p->width;
  ischan = p->ischan;
	inst = p->inst;
	reg = p->reg;
	root_id = p->root_id;
  connection = p->connection;
  type = 0;
  o_type = p->o_type;
  owner.sm = p->owner.sm;
  owner.smi = p->owner.smi;
}

Port::Port(int dir_, int width_, int chan_, int reg_){
  dir = dir_;
  width = width_;
  ischan = chan_;
	inst = 0;
	reg = reg_;
	root_id = NULL;
  connection = NULL;
  type = 0;
  o_type = 0;
  owner.sm = NULL;
  owner.smi = NULL;
}

Port::Port(int dir_, int width_, int chan_, int reg_, 
						ValueIdx *rid_, act_connection *c_){
  dir = dir_;
  width = width_;
  ischan = chan_;
	inst = 0;
	reg = reg_;
	root_id = rid_;
  connection = c_;
  type = 0;
  o_type = 0;
  owner.sm = NULL;
  owner.smi = NULL;
}

Port::Port(int dir_, int width_, int chan_, int reg_, 
						ValueIdx *rid_, ActId *pn_, act_connection *c_){
  dir = dir_;
  width = width_;
  ischan = chan_;
	inst = 0;
	reg = reg_;
	root_id = rid_;
  connection = c_;
  port_name = pn_;
  type = 0;
  o_type = 0;
  owner.sm = NULL;
  owner.smi = NULL;
}
Port::~Port() {}

/*
 *  Variable Class
 */
int Variable::GetDimNum(){	return dim.size(); }
act_connection *Variable::GetCon(){	return con; }
void Variable::AddDimension(int d) { dim.push_back(d); }
int Variable::IsChan() { return ischan; }
int Variable::IsPort() { return isport; }

Variable::Variable() {
  type = 0;
	ischan = 0;
	isport = 0;
  isdyn = 0;
	id = NULL;
}

Variable::Variable(int type_, int ischan_, int isport_, int isdyn_,
										act_connection *con_, ActId *id_) {
  type = type_;
	ischan = ischan_;
	isport = isport_;
  isdyn = isdyn_;
	id = id_;
	con = con_;
}

}
