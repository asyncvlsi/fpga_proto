namespace fpga {

class Condition;
class StateMachine;
class StateMachineInst;

class CHPData {
public:

  CHPData();
  CHPData(int, Process *, StateMachine *, Condition *, Condition *, ActId *, Expr *);
  CHPData(int, Process *, StateMachine *, Condition *, Condition *, ActId *, ActId *);
  ~CHPData();

  int GetType();
  Condition *GetCond();
  Process *GetActScope();
  StateMachine *GetScope();
  ActId *GetId();

  void PrintPlain();
  void PrintVerilog(int, std::string&);
  void GetSuffix(std::string&, int);
  void PrintVerilogHSrhs(std::string&);
  void PrintVerilogAssignment(std::string &);

  void PrintVerilogIfCond(std::string &);
  void PrintVerilogRHS(std::string &);

private:

  //at which state data processing happens
  Condition *cond;

  int type; //0 - expression
            //1 - recv
            //2 - send
            //3 - function
            //4 - unused (only reset to 0)
            //5 - internal communication send
            //6 - internal communication recv
            //7 - send a structure

  ActId *id;  //name of the variable assigning to

  union {
    //assigning an expression
    struct {
      Expr *e; 
    } assign;
 
    //assigning channel to a variable
    struct {
      Condition *up_cond;
      ActId *chan;
    } recv;

    //assigning expression to a channel
    struct {
      Condition *up_cond;
      Expr *se;
    } send;

    //assigning variable to a channel
    struct {
      Condition *up_cond;
      ActId *chan;
    } send_struct;

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
  Port(int, int, int, int, ValueIdx *, ActId *, act_connection *);
  Port(int, int, int, int);
  Port(Port *);
  ~Port();

  int GetDir();
  act_connection *GetCon();
  ValueIdx *GetVx();
  int GetChan();
  int GetInst();

  void SetType(int t) { type = t; };
  void SetInst();
  void SetCtrlChan();

  void PrintVerilog();
  void PrintName(std::string &);

  //Extra
  int GetOwnerType() { return o_type; };
  StateMachine *GetSM() { return owner.sm; };
  StateMachineInst *GetSMI() { return owner.smi; };
  void SetOwner(StateMachine *sm_) { 
    o_type = 0;
    owner.sm = sm_; 
  };
  void SetOwner(StateMachineInst *smi_) { 
    o_type = 1;
    owner.smi = smi_; 
  };
  void SetId(ActId *id) { glue_port_name = id; };
  ActId *GetId() { return glue_port_name; };
  void FlipDir() { dir = dir == 0 ? 1 : 0; };
  void PrintAsGlue(std::string &);
  int GetWidth() { return width; };

private:

  int dir;  //0 - output; 1 - input
  int width;
  int ischan; //0 - no, 1 - yes, 2 - control chan(no data)
  int inst;
  int reg; //0 - wire, 1 - reg
  int type; //0 - normal, 1 - structure (for channel prints to have
            // only one hand shake)

  ValueIdx *root_id;
  act_connection *connection;
  ActId *port_name;

  //Extra
  int o_type;
  union {
    StateMachine *sm;
    StateMachineInst *smi;
  } owner;
  ActId *glue_port_name;

};

void PrintExpression(Expr *, StateMachine *, std::string &);

class Variable {
public:

  Variable();
  Variable(int, int, int, int, act_connection *, ActId *);
  
  void AddDimension(int);

  act_connection *GetCon();
  int GetDimNum();
  int GetType() { return type; }
  inline int getDimSize(int n) { return dim[n]; }
  inline void setDimSize(int n, int ns) { dim[n] = ns; }

  int IsChan();
  int IsPort();

  inline void MkDyn() { isdyn = 1; }

  void PrintVerilog();

private:

  int type;   //0 - reg, 1 - wire, 2 - inst-inst wire, 3 - internal buffer, 4/5 - glue
              //6 - internal buffer hs only(for structures)
  int ischan; //0 - no, 1 - yes
  int isport; //0 - no, 1 - yes
  int isdyn;  //0 - no, 1 - yes

  std::vector<int> dim; //vector of array dimentions width

  act_connection *con;
  ActId *id;

};

class Arbiter {
public:

  Arbiter();
  ~Arbiter();

  void AddElement(Condition *c);

  void PrintArbiter(FILE *);
  void PrintInst(int n);

private:
  
  std::vector<Condition *> a;

};


}
