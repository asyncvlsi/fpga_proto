#include <vector>
#include <string.h>
#include <act/act.h>

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

  void PrintPlain();
  //f=0 - Name
  //f=1 - Name + Expression 
  //f=2 - Expression
  void PrintVerilog(int f);

  //might be not the best idea, but I need it... :(
  State *GetState();

private:
  
  void PrintExpr(Expr *);
  void PrintScope(StateMachine *sc, int);

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

  int GetType();
  int GetNum();
  std::vector<std::pair<State *, Condition *>> GetNextState();
  StateMachine *GetPar();

  void PrintPlain(int p = 1);
  void PrintVerilog(int p = 1);
  void PrintVerilogName(int);
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
  void AddData(act_connection*, Data *);
  void AddHS  (act_connection*, Data *);
  void AddPort(Port *);
  void AddVar(Variable *);
	void AddInst(StateMachineInst *);
	void AddArb(Arbiter *a);

  int GetSize();
  int GetNum();
  int GetGN();
  int GetSN();
  int GetCN();
  int GetCCN();
  int GetKids();
  StateMachine *GetPar();
  StateMachine *GetNext();
  std::vector<Variable *> GetVars();
  std::vector<Port *> GetPorts();
  Process *GetProc();
	std::vector<StateMachineInst *> GetInst();
  inline int GetType() { return top->GetType(); };

  void PrintParent(StateMachine *, int);
  void PrintPlain();
  void PrintVerilog();
  void PrintVerilogHeader(int sv);

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
  std::vector<Variable *> vars;
	//std::map<ValueIdx *, std::vector<Variable *> > vm;
	std::map<act_connection *, Variable *> vm;

  std::vector<Condition *> guard_condition;
  std::vector<Condition *> state_condition;
  std::vector<Condition *> commu_condition;
  std::vector<Condition *> comma_condition;

	std::vector<Arbiter *> arb;

  void PrintPlainState(std::vector<std::pair<State *, Condition *>> s);
  void PrintVerilogState(std::vector<std::pair<State *, Condition *>> s);
  void PrintVerilogWires();
  void PrintVerilogVars();
  void PrintVerilogParameters();
  void PrintSystemVerilogParameters(int);

  std::vector<StateMachine *> csm;

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

/*
 *  Data path class
 */

class Data {
public:

  Data();
  Data(int, int, int, Process *, StateMachine *, Condition *, Condition *, ActId *, Expr *);
  Data(int, int, int, Process *, StateMachine *, Condition *, Condition *, ActId *, ActId *);
  ~Data();

  int GetType();
  Condition *GetCond();
  Process *GetActScope();
  StateMachine *GetScope();
  ActId *GetId();

  void PrintPlain();
  void PrintVerilogCondition(int);
  void PrintVerilogConditionUP(int);
  void PrintVerilogAssignment();
  void PrintVerilogHS(int);

private:

  //at which state data processing happens
  Condition *cond;

  int type; //0 - expression
            //1 - recv
            //2 - send
            //3 - function

  int up; //array slice upper boundary
  int dn; //array slice lower boundary

  ActId *id;  //name of the variable assigning to

  union {
    //assigning an expression
    struct {
      Expr *e; 
    } assign;
 
    //assigning channel value
    struct {
      Condition *up_cond;
      ActId *chan;
    } recv;

    //assigning expression to the channel
    struct {
      Condition *up_cond;
      Expr *se;
    } send;

    //assigning function value
    struct {
      mstring_t *name;
    } func;
  } u;

  StateMachine *scope;
  Process *act_scope;

  unsigned int printed:1;

};

class Port {
public:

  Port();
  Port(int, int, int, int, ValueIdx *, act_connection *);
  ~Port();

  int GetDir();
  act_connection *GetCon();
	ValueIdx *GetVx();
	int GetChan();
	int GetInst();

	void SetInst();
	void SetCtrlChan();

  void Print();
	void PrintName(int func = 0);

private:

  int dir;  //0 - output; 1 - input
  int width;
  int ischan; //0 - no, 1 - yes, 2 - control chan(no data)
	int inst;
	int reg; //0 - wire, 1 - reg

	ValueIdx *root_id;
  act_connection *connection;

};

void PrintExpression(Expr *);

class Variable {
public:

  Variable();
  Variable(int, int);
  Variable(int, int, ValueIdx *);
  Variable(int, int, ValueIdx *, act_connection *);
  Variable(int, int, int, ValueIdx *, act_connection *);
  
	void AddDimension(int);

	ValueIdx *GetId();
  act_connection *GetCon();
	int GetDimNum();
  inline int getDimSize(int n) { return dim[n]; }
  inline void setDimSize(int n, int ns) { dim[n] = ns; }

	int IsChan();
	int IsPort();

  inline void MkDyn() { isdyn = 1; }

  void PrintVerilog();

private:

  int type;   //0 - reg, 1 - wire
	int ischan; //0 - no, 1 - yes
	int isport; //0 - no, 1 - yes
  int isdyn;  //0 - no, 1 - yes

	std::vector<int> dim; //vector of array dimentions width

	ValueIdx *vx;
  act_connection *id;  //name

};

class Arbiter {
public:

	Arbiter();
	~Arbiter();

	void AddElement(Condition *c);

	void PrintArbiter();
	void PrintInst(int n);

private:
	
	std::vector<Condition *> a;

};

}


