#include <vector>
#include "act/act.h"

namespace fpga {

class Condition;
class StateMachine;

struct Comma {
  int type; //0 - ANDed
            //1 - ORed
            //2 - ANDed with negation
  std::vector <Condition *> c;
};

class Condition {
public:

  Condition();
  Condition(ActId *v_, int num_);
  Condition(Expr *e_, int num_);
  Condition(int st_num_, int num_);
  Condition(Comma *c_, int num_);

  int GetType();
  int GetNum();

  void PrintPlain(); 
  void PrintVerilog();

private:
  
  void PrintExpr(Expr *);

  int type; //0 - communication completion
            //1 - selection/loop guard
            //2 - statement completion (1 cycle operation)
            //3 - comma

  int num;

  union {

    //variable completing communication
    ActId *v;

    //pointer to the guard expression
    Expr *e;

    //int state number to complete
    int st_num;

    //comma type
    //generic type for next state conditions
    //its a vector where all conditions are
    //ANDed;
    Comma *c;

  } u;
 
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

  void PrintPlain();
  void PrintVerilog();
  bool isPrinted();

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

  void SetFirstState(State *s);
  void SetProcess(Process *p_);
  void SetNumber(int n);
  void SetNext(StateMachine *smn);
  void SetParent(StateMachine *psm);

  void AddCondition(Condition *c);
  void AddSize();
  void AddKid(StateMachine *sm);

  int GetSize();
  int GetNum();
  int GetGN();
  int GetSN();
  int GetCN();
  int GetCCN();
  int GetKids();
  StateMachine *GetPar();
  StateMachine *GetNext();

  void PrintVerilog();
  void PrintPlain();

private:

  Process *p;

  int number;

  int size;

  int guard_num;
  int st_num;
  int commun_num;
  int comma_num;

  State *top;

  std::vector<Condition *> guard_condition;
  std::vector<Condition *> state_condition;
  std::vector<Condition *> commu_condition;
  std::vector<Condition *> comma_condition;

  void PrintState(std::vector<std::pair<State *, Condition *>> s);

  //list of child statemachines
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

private:

  StateMachine *hd, *tl;

};

CHPProject *build_machine (Act *, Process *);

} 
