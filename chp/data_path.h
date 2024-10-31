namespace fpga {

class Condition;
class StateMachine;
class StateMachineInst;

class CHPData {
public:

  CHPData();
  CHPData(int, int, int, Process *, StateMachine *, Condition *, Condition *, ActId *, Expr *);
  CHPData(int, int, int, Process *, StateMachine *, Condition *, Condition *, ActId *, ActId *);
  ~CHPData();

  int GetType();
  Condition *GetCond();
  Process *GetActScope();
  StateMachine *GetScope();
  ActId *GetId();

  void PrintPlain();
  void PrintVerilog(int, std::string&);
  void PrintVerilogHSlhs(std::string&, int);
  void PrintVerilogHSrhs(std::string&);
  void PrintVerilogAssignment(std::string &);

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
  Port(Port *);
  ~Port();

  int GetDir();
  act_connection *GetCon();
  ValueIdx *GetVx();
  int GetChan();
  int GetInst();

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

  ValueIdx *root_id;
  act_connection *connection;

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
  Variable(int, int);
  Variable(int, int, ValueIdx *);
  Variable(int, int, ValueIdx *, act_connection *);
  Variable(int, int, int, ValueIdx *, act_connection *);
  Variable(int, int, int, int, ValueIdx *, act_connection *);
  
  void AddDimension(int);

  ValueIdx *GetId();
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

  void PrintArbiter(FILE *);
  void PrintInst(int n);

private:
  
  std::vector<Condition *> a;

};


}
