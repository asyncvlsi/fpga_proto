#include <vector>
#include <string>
#include <string.h>
#include <act/act.h>
#include "data_path.h"

namespace fpga {

class Data;
class Port;
class Variable;
class Arbiter;

class Condition;
class State;
class StateMachine;

//Comma is a general type for multi 
//condition conditions representation
struct Comma {
  int type; //0 - ANDed
            //1 - ORed
            //2 - ANDed with negation
  std::vector <Condition *> c;
};

//Condition class is for state machine
//state switching conditions
class Condition {
public:

  Condition();
  Condition(ActId *v_, int num_, StateMachine *sc);
  Condition(Expr *e_,  int num_, StateMachine *sc);
  Condition(Expr *e_,  int num_, StateMachine *sc, int a);
  Condition(State *s_, int num_, StateMachine *sc);
  Condition(Comma *c_, int num_, StateMachine *sc);
  Condition(bool *con_,int num_, StateMachine *sc);

  int GetType();
  int GetNum();
  StateMachine *GetScope();

  void SetScope(StateMachine *sm) { scope = sm; }
  void SetNum(int _n) { num = _n; }

  inline void MkArb(){ type = 4; };

  void PrintPlain();
  void PrintVerilogDecl(std::string &);
  void PrintVerilogDeclRaw(std::string &);
  void PrintVerilogExpr(std::string &);
  void PrintScopeParam(StateMachine *sc, std::string &);
  void PrintScopeVar(StateMachine *, std::string &);

  //might be not the best idea, but I need it... :(
  State *GetState();

private:
  
  void PrintExpr(Expr *, std::string &);

  int type; //0 - communication completion
            //1 - selection/loop guard
            //2 - statement completion (1 cycle operation)
            //3 - comma
            //4 - arbitrated guard

  int fo;   //0 - no max fanout  applied
            //1 - max fanout attribute applied

  int num;  //condition number for each paticular type

  union {
    //constant condition
    bool *con;

    //variable completing communication
    ActId *v;

    //pointer to the guard expression
    Expr *e;

    //int state number to complete
    State* s;

    //comma type
    //generic type for next state conditions
    //its a vector where all conditions are
    //ANDed;
    Comma *c;

  } u;

  StateMachine *scope;  //parent state machine
};

//State is a sequence of statements
//can be either simple sequence between
//semicolons or a sequence in the 
//selection or loop statment
class State {
public:

  State();
  State(int type_, int number_, StateMachine *par_);
  ~State();

  void AddNextState(std::pair<State *, Condition *> s);
  void AddNextStateRaw(State *, Condition *);

  int GetType();
  int GetNum();
  std::vector<std::pair<State *, Condition *>> GetNextState();
  StateMachine *GetPar();

  void PrintPlain(int p = 1);
  void PrintVerilog(std::string &);
  void PrintScopeVar(StateMachine *p, std::string &);
  void PrintScopeParam(StateMachine *p, std::string &);
  bool isPrinted();
  void PrintType();

private:

  //state type same as CHP types
  //but without semicolon
  int type;

  //state number inside parent statemachine
  int number;

  //list of next states in the same 
  //order as next condition
  std::vector<std::pair<State *, Condition *>> ns;

  StateMachine *par;

  //just to avoid loop
  unsigned int printed:1;

  void PrintParent(StateMachine *p, int);
};

class StateMachineInst {
public:

  StateMachineInst();
  StateMachineInst(Process *, ValueIdx *, char *, std::vector<Port *>&);
  ~StateMachineInst();

  void SetSM(StateMachine *);
  void SetCtrlChan(int i);

  Process *GetProc();
  std::vector<Port *> GetPorts();

  void PrintVerilog();

private:

  Process *p;

  ValueIdx *name;

  char *array;

  std::vector<Port *> ports;

  StateMachine *sm;

};

//State machine which controls
//datapath of the circuit model
class StateMachine {
public:

  StateMachine();
  StateMachine(State *s, int n, Process *p_);
  StateMachine(State *s, int n, StateMachine *par_);
  ~StateMachine();

  bool IsEmpty();
  int IsPort(act_connection *);

  void SetFirstState(State *s);
  void SetProcess(Process *p_);
  void SetNumber(int n);
  void SetNext(StateMachine *smn);
  void SetParent(StateMachine *psm);

  void AddCondition(Condition *c);
  void AddSize();
  void AddKid(StateMachine *sm);
  void AddSib(StateMachine *sm);
  void AddData(act_connection*, Data *);
  void AddHS  (act_connection*, Data *);
  void AddPort(Port *);
  void AddVar(Variable *);
  void AddInst(StateMachineInst *);
  void AddInstPortPair(act_connection *, Port *);
  void AddArb(Arbiter *a);

  int GetSize();
  int GetNum();
  int GetGN();
  int GetSN();
  int GetCN();
  int GetCCN();
  int GetKids();
  int GetSibs();
  StateMachine *GetPar();
  StateMachine *GetNext();
  std::map<act_connection*, std::vector<Data *>> GetData() { return data; };
  std::map<act_connection*, std::vector<Data *>> GetHSData() { return hs_data; };
  std::vector<Variable *> GetVars();
  inline Variable *GetVarRaw(act_connection *c) { return vm[c]; };
  inline int GetVarType(act_connection *c) { return vm[c]->GetType(); };
  std::vector<Port *> GetPorts();
  Process *GetProc();
  std::vector<StateMachineInst *> GetInst();
  std::map<act_connection *, std::vector<Port *>> GetInstPorts() { return _ports; };
  inline int GetType() { return top->GetType(); };

  void PrintParent(StateMachine *, int);
  void PrintScopeVar(StateMachine *, std::string &);
  void PrintScopeParam(StateMachine *, std::string &);
  void PrintPlain();
  void PrintVerilog();
  void PrintVerilogBody();
  void PrintVerilogHeader(int sv);
  void PrintVerilogWires();
  void PrintVerilogVars();
  void PrintVerilogData();
  void PrintVerilogDataHS();

  void Clear();

  StateMachine *Next() { return next; }

private:

  Process *p;

  int number;

  int size;

  int guard_num;
  int st_num;
  int commun_num;
  int comma_num;

  State *top;

  std::vector<StateMachineInst *> inst;

  std::map<act_connection*, std::vector<Data *>> data;
  std::map<act_connection*, std::vector<Data *>> hs_data;

  std::vector<Port *> ports;
  std::map<act_connection *, std::vector<Port *>> _ports;  //connection to inst ports mapping
  std::vector<Variable *> vars;
  std::map<act_connection *, Variable *> vm;

  std::vector<Condition *> guard_condition;
  std::vector<Condition *> state_condition;
  std::vector<Condition *> commu_condition;
  std::vector<Condition *> comma_condition;

  std::vector<Arbiter *> arb;

  void PrintPlainState(std::vector<std::pair<State *, Condition *>> s);
  void PrintVerilogState(std::vector<std::pair<State *, Condition *>> s, std::string &);
  void PrintVerilogParameters();
  void PrintSystemVerilogParameters(int);

  std::vector<StateMachine *> csm; //children
  std::vector<StateMachine *> ssm; //siblings

  StateMachine *next;
  StateMachine *par;

};


class CHPProject {
public:

  CHPProject();
  ~CHPProject();

  void Append(StateMachine *sm);

  void PrintPlain();
  void PrintVerilog(Act *, int , FILE *);

  StateMachine *Head();
  StateMachine *Next();

private:

  StateMachine *hd, *tl;

};

CHPProject *build_machine (Act *, Process *, int);

}
