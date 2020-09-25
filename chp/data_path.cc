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

int Port::GetDir() {
  return dir;
}

act_connection *Port::GetCon(){
  return connection;
}

void Port::Print(){
  if (dir == 0) {
    if (ischan) {
      fprintf(stdout, "\t,output reg\t");
      connection->toid()->Print(stdout);
      fprintf(stdout, "_valid\n");
      fprintf(stdout, "\t,input     \t");
      connection->toid()->Print(stdout);
      fprintf(stdout, "_ready\n");
    }
    fprintf(stdout, "\t,output reg\t");
  } else {
    if (ischan) {
      fprintf(stdout, "\t,output reg\t");
      connection->toid()->Print(stdout);
      fprintf(stdout, "_ready\n");
      fprintf(stdout, "\t,input     \t");
      connection->toid()->Print(stdout);
      fprintf(stdout, "_valid\n");
    }
    fprintf(stdout, "\t,input     \t");
  }
  fprintf(stdout, "[%i:0]\t", width-1);
  connection->toid()->Print(stdout);
}

Port::Port(){
  dir = 0;
  width = 0;
  ischan = 0;
  connection = NULL;
}

Port::Port(int dir_, int width_, int chan_, act_connection *c_){
  dir = dir_;
  width = width_;
  ischan = chan_;
  connection = c_;
}

Port::~Port() {}

/*
 *  Variable Class
 */

act_connection *Variable::GetCon() {
  return id;
}

Variable::Variable() {
  type = 0;
  width = 0;
  id = 0;
}

Variable::Variable(int type_, int width_, act_connection *id_) {
  type = type_;
  width = width_;
  id = id_;
}


}
