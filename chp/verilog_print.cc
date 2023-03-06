#include <act/act.h>
#include <act/passes/booleanize.h>
#include <act/state_machine.h>
#include <string>
#include <math.h>

namespace fpga {

FILE *output_file = stdout;
static ActBooleanizePass *BOOL = NULL;

void PrintExpression(Expr *, StateMachine *, std::string &);

void get_module_name (Process *p, std::string &str) {

  const char *mn = p->getName();
  std::string tmp1 = "";
  std::string tmp2 = "";

  if (p->getns() && p->getns()->Parent()) {
    ActNamespace *an = p->getns();
    tmp1 = tmp2 + an->getName() + "_" + tmp1;
    an = an->Parent();
    while (an->Parent()) {
      tmp1 = tmp2 + an->getName() + "_" + tmp1;
      an = an->Parent();
    }
  }
  str += tmp1;

  int len = strlen(mn);

  for (auto i = 0; i < len; i++) {
    if (mn[i] == 0x3c) {
      str += "_";
      continue;
    } else if (mn[i] == 0x3e) {
      str += "_";
      continue;
    } else if (mn[i] == 0x2c) {
      str += "_";
      continue;
    } else {
      str += mn[i];
    }
  }

  return;
}

void print_array_ref (ActId *id, StateMachine *scope, std::string &str) {

  str += id->getName();
  if (id->arrayInfo()) {
    for (int i = 0; i < id->arrayInfo()->nDims(); i++) {
      str += " [";
      PrintExpression(id->arrayInfo()->getDeref(i), scope, str);
      str += " ]";
    }
  }

  return;
}

void PrintExpression(Expr *e, StateMachine *scope, std::string &str) {
  if (e->type == E_NOT || e->type == E_COMPLEMENT) { str += " ~("; } 
  else if (e->type == E_UMINUS) { str += " -("; } 
  else if (e->type == E_CONCAT || e->type == E_COMMA) { str += "{"; } 
  else { str += "("; }
  switch (e->type) {
    case (E_AND): {
      PrintExpression(e->u.e.l, scope, str); str += " & "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_OR): {
      PrintExpression(e->u.e.l, scope, str); str += " | "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_NOT): {
      PrintExpression(e->u.e.l, scope, str);
      break;
    }
    case (E_PLUS): {
      PrintExpression(e->u.e.l, scope, str); str += " + "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_MINUS): {
      PrintExpression(e->u.e.l, scope, str); str += " - "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_MULT): {
      PrintExpression(e->u.e.l, scope, str); str += " * "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_DIV): {
      PrintExpression(e->u.e.l, scope, str); str += " / "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_MOD): {
      PrintExpression(e->u.e.l, scope, str); str += " % "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_LSL): {
      PrintExpression(e->u.e.l, scope, str); str += " << "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_LSR): {
      PrintExpression(e->u.e.l, scope, str); str += " >> "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_ASR): {
      PrintExpression(e->u.e.l, scope, str); str += " >>> "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_UMINUS): {
      PrintExpression(e->u.e.l, scope, str);
      break;
    }
    case (E_INT): {
      str = str + "32'd" + std::to_string(e->u.ival.v);
      break;
    }
    case (E_VAR): {
      char tmp[1024];
      ActId *id;
      id = (ActId *)e->u.e.l;
      act_boolean_netlist_t *bnl;
      bnl = BOOL->getBNL(scope->GetProc());
      act_dynamic_var_t *dv;
      dv = BOOL->isDynamicRef(bnl, id);
      str += "\\";
      if (!dv) { 
        id->sPrint(tmp, 1024);
        str += tmp;
      } else {
        print_array_ref(id, scope, str);
      }
      break;
    }
    case (E_QUERY): {
      PrintExpression(e->u.e.l, scope, str); str += " ? ";
      PrintExpression(e->u.e.r->u.e.l, scope, str); str += " : \n\t\t"; PrintExpression(e->u.e.r->u.e.r, scope, str);
      break;
    }
    case (E_LPAR): {
      str += "LPAR\n";
      break;
    }
    case (E_RPAR): {
      str += "RPAR\n";
      break;
    }
    case (E_XOR): {
      PrintExpression(e->u.e.l, scope, str); str += " ^ "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_LT): {
      PrintExpression(e->u.e.l, scope, str); str += " < "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_GT): {
      PrintExpression(e->u.e.l, scope, str); str += " > "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_LE): {
      PrintExpression(e->u.e.l, scope, str); str += " <="; PrintExpression(e->u.e.r, scope, str);
      break;  
    }
    case (E_GE): {
      PrintExpression(e->u.e.l, scope, str); str += " >= "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_EQ): {
      PrintExpression(e->u.e.l, scope, str); str += " == "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_NE): {
      PrintExpression(e->u.e.l, scope, str); str += " != "; PrintExpression(e->u.e.r, scope, str);
      break;
    }
    case (E_TRUE): {
      str += " 1'b1 ";
      break;  
    }
    case (E_FALSE): {
      str += " 1'b0 ";
      break;
    }
    case (E_COLON): {
      str += " : ";
      break;
    }
    case (E_PROBE): {
      char tmp[1024];
      ActId *id;
      Scope *act_scope;
      act_scope = scope->GetProc()->CurScope();
      id = (ActId *)e->u.e.l;
      act_connection *cc = id->Canonical(act_scope);
      StateMachine *sm = scope->GetPar();
      if (sm) {
        while (sm->GetPar()) { sm = sm->GetPar(); }
      }
      str += "\\";
      id->sPrint(tmp,1024);
      str += tmp;
      if (sm) {
        if (sm->IsPort(cc) == 1) {
          str +="_ready";
        } else if (sm->IsPort(cc) == 2){
          str +="_valid";
        }
      } else {
        if (scope->IsPort(cc) == 1) {
          str +="_ready";
        } else if (scope->IsPort(cc) == 2){
          str +="_valid";
        }
      }
      break;
    }
    case (E_COMMA): {
      PrintExpression(e->u.e.l, scope, str);
      if (e->u.e.r) {
        str += " ,"; PrintExpression(e->u.e.r, scope, str);
      }
      break;
    }
    case (E_CONCAT): {
      PrintExpression(e->u.e.l, scope, str);
      if (e->u.e.r) {
        str += " ,"; PrintExpression(e->u.e.r, scope, str);
      }
      break;
    }
    case (E_BITFIELD): {
      unsigned int l;
      unsigned int r;
      l = e->u.e.r->u.e.r->u.ival.v;
      if (e->u.e.r->u.e.l) {
        r = e->u.e.r->u.e.l->u.ival.v;
      } else {
        r = l;
      }
      print_array_ref(((ActId *)e->u.e.l), scope, str);
      if (l!=r) {
        str = str + "[" + std::to_string(l) + ":" + std::to_string(r) + "]";
      } else {
        str = str + "[" + std::to_string(r) + "]";
      }
      break;
    }
    case (E_COMPLEMENT): {
      PrintExpression(e->u.e.l, scope, str);
      break;
    }
    case (E_REAL): {
      str += std::to_string(e->u.ival.v);
      break;
    }
    case (E_ANDLOOP): {
      str += "ANDLOOP you mean?\n";
      break;
    }
    case (E_ORLOOP): {
      str += "ORLOOP you mean?\n";
      break;
    }
    case (E_BUILTIN_INT): {
      PrintExpression(e->u.e.l, scope, str);
      break;
    }
    case (E_BUILTIN_BOOL): {
      PrintExpression(e->u.e.l, scope, str);
      break;
    }
    case (E_RAWFREE): {
      str += "RAWFREE\n";
      break;
    }
    case (E_END): {
      str += "END\n";
      break;
    }
    case (E_NUMBER): {
      str += "NUMBER\n";
      break;
    }
    case (E_FUNCTION): {
      str += "FUNCTION\n";
      break;
    }
    default: {
      str += "Whaaat?! "; str += std::to_string(e->type); str += "\n";
      break;
    }
  }
  if (e->type == E_CONCAT || e->type == E_COMMA) { str += " }"; } 
  else { str += " )"; }

  return;
}

void Condition::PrintScopeVar(StateMachine *sc, std::string &name){

  name = name + "sm" + std::to_string(sc->GetNum()) + "_";
  if (sc->GetPar()) { PrintScopeVar(sc->GetPar(), name); }

  return;
}

void Condition::PrintScopeParam(StateMachine *sc, std::string &name) {

  name = name + "SM" + std::to_string(sc->GetNum()) + "_";
  if (sc->GetPar()) { PrintScopeParam(sc->GetPar(), name); }

  return;
}

void Condition::PrintExpr(Expr *e, std::string &str) {
  PrintExpression(e, scope, str);
  return;
}

void Condition::PrintVerilogExpr(std::string &name) {

  StateMachine *sm;
  char tmp[1024];

  name += "assign ";
  PrintScopeVar(scope, name);
  switch (type) {
    case (0) :
      u.v->sPrint(tmp,1024);
      name = name + "commu_compl_" + std::to_string(num) + " = \\" + tmp + "_valid & \\" + tmp + "_ready ";
      break;
    case (1) :
      name = name + "guard_" + std::to_string(num) + " = ";
      PrintExpr(u.e, name);
      name += " ? 1'b1 : 1'b0 ";
      break;
    case (2) :
      sm = u.s->GetPar();
      name = name + "state_cond_" + std::to_string(num) + " = ";
      PrintScopeVar(sm,name);
      name = name + "state == "; 
      PrintScopeParam(sm,name);
      name = name + "STATE_" + std::to_string(u.s->GetNum());
      break;
    case (3) :
      name = name + "cond_" + std::to_string(num) + " = ";
      for (auto cc : u.c->c) {
        if (u.c->type == 2) { name += "!"; }
        PrintScopeVar(cc->GetScope(),name);
        if (cc->GetType() == 0) {
          name = name + "commu_compl_" + std::to_string(cc->GetNum());
        } else if (cc->GetType() == 1 || cc->GetType() == 4) {
          name = name + "guard_" + std::to_string(cc->GetNum());
        } else if (cc->GetType() == 2) {
          State *ss = cc->GetState();
          sm = ss->GetPar();
          name = name + "state_cond_" + std::to_string(cc->GetNum());
        } else {
          name = name + "cond_" + std::to_string(cc->GetNum());
        }
        if (cc != u.c->c[u.c->c.size()-1]) {
          if (u.c->type != 1) {
            name += " & ";
          } else {
            name += " | ";
          }
        }
      }
      break;
    case (4) :
      name = name + "excl_guard_" + std::to_string(num) + " = ";
      PrintExpr(u.e, name);
      name += " ? 1'b1 : 1'b0 ";
      break;
    default :
      fatal_error("!!!\n");
  }
  name += ";\n";

  return;
}

void Condition::PrintVerilogDecl(std::string &name) {

  name += "wire ";
  PrintScopeVar(scope, name);
  switch (type) {
  case (0) :
    name += "commu_compl_";
    break;
  case (1) :
    name +=  "guard_";
    break;
  case (2) :
    name +=  "state_cond_";
    break;
  case (3) :
    name +=  "cond_";
    break;
  case (4) :
    name +=  "excl_guard_";
   name += std::to_string(num);
   name += ";\n";
   name += "wire ";
   PrintScopeVar(scope, name);
   name +=  "guard_";
    break;
  default :
    fatal_error("!!!\n");
  }
  name += std::to_string(num);
  name += ";\n";

  return;
}

void Condition::PrintVerilogDeclRaw(std::string &name) {

  PrintScopeVar(scope, name);
  switch (type) {
  case (0) :
    name += "commu_compl_";
    break;
  case (1) :
    name +=  "guard_";
    break;
  case (2) :
    name +=  "state_cond_";
    break;
  case (3) :
    name +=  "cond_";
    break;
  case (4) :
    name +=  "excl_guard_";
    break;
  default :
    fatal_error("!!!\n");
  }
  name += std::to_string(num);

  return;
}

void Variable::PrintVerilog() {

  char tmp[1024];
  id->toid()->sPrint(tmp, 1024);

  std::string var;

  if (type == 0) { var = "reg\t"; } 
  else { var = "wire\t"; }

  if (dim[0] >= 1) { var = var + "[" + std::to_string(dim[0]) + ":0]"; }

  var = var + "\t\\" + tmp;

  for (auto i = 1; i < dim.size(); i++) {
    if (isdyn == 1) { var += " "; }
    var = var + "[" + std::to_string(dim[i]-1) + ":0]";
  }
  var += " ;\n";
  
  if (ischan != 0) {
    if (type == 0) { var += "reg\t\\"; } 
    else { var += "wire\t\\"; }
    var = var + tmp + "_valid ;\n";

    if (type == 0 || type == 2) { var += "wire\t\\"; } 
    else { var += "reg\t\\"; }
    var = var + tmp + "_ready ;\n";
  } 

  fprintf(output_file, "%s", var.c_str());
  var.clear();

  return;
}

void StateMachine::PrintVerilogVars() {
  for (auto v : vars) {
    if (v->IsPort() == 0) {
      v->PrintVerilog();
    }
  }
  fprintf(output_file, "\n");

  return;
}

void StateMachine::PrintVerilogWires() {

  std::string wire;

  for (auto c : csm) {
    c->PrintVerilogWires();
  }

  for (auto cc : guard_condition) {
    cc->PrintVerilogDecl(wire);
  }

  for (auto cc : state_condition) {
    cc->PrintVerilogDecl(wire);
  }

  for (auto cc : commu_condition) {
    cc->PrintVerilogDecl(wire);
  }

  for (auto cc : comma_condition) {
    cc->PrintVerilogDecl(wire);
  }
  fprintf(output_file, "%s\n", wire.c_str());
  wire.clear();

  return;
}

void State::PrintScopeParam(StateMachine *p, std::string &name) {

  name = name + "SM" + std::to_string(p->GetNum()) + "_";
  if (p->GetPar()) {
    PrintScopeParam(p->GetPar(), name);
  }

  return;
}

void State::PrintScopeVar(StateMachine *p, std::string &name) {

  name = name + "sm" + std::to_string(p->GetNum()) + "_";
  if (p->GetPar()) {
    PrintScopeVar(p->GetPar(), name);
  }

  return;
}

void StateMachine::PrintVerilogParameters(){

  std::string name;

  if (top) {
    name = "reg [31:0] ";
    top->PrintScopeVar(this, name);
    name += "state = 0;\n";
    for (auto i = 0; i < size; i++) {
      name += "localparam ";
      top->PrintScopeParam(this, name);
      name = name + "STATE_" + std::to_string(i) + " = " + std::to_string(i) + ";\n";
    }
    name += "\n";
    fprintf(output_file, "%s", name.c_str());
    for (auto c : csm) {
      c->PrintVerilogParameters();
    }
  }

  return;
}

void Port::PrintVerilog() {

  std::string port;
  char tmp[1024];
  connection->toid()->sPrint(tmp,1024);

  if (dir == 0) {
    if (ischan != 0) {
      if (reg) {
        port = port + "\t,output reg\t\\" + tmp;
      } else {
        port = port + "\t,output\t\\" + tmp;
      }
      port = port + "_valid\n\t,input\t\t\\" + tmp + "_ready\n";
    }
    if (ischan != 2) {
      if (reg) {
        port = port + "\t,output reg\t";
      } else {
        port = port + "\t,output\t";
      }
    }
  } else {
    if (ischan != 0)  {
      if (reg) {
        port = port + "\t,output reg\t\\" + tmp;
      } else {
        port = port + "\t,output\t\\" + tmp;
      }
      port = port + "_ready\n\t,input\t\t\\" + tmp + "_valid\n";
    }
    if (ischan != 2) {
      port += "\t,input\t\t";
    }
  }
  if (ischan != 2) {
    port = port + "[" + std::to_string(width-1) + ":0]\t\\" + tmp;
  }
  fprintf(output_file, "%s\n", port.c_str());
  port.clear();

  return;
}

void StateMachine::PrintVerilogHeader(int sv) {
  std::string header;
  if (p) {
    header += "`timescale 1ns/1ps\n\n";
    header += "module \\";
    get_module_name(p, header);
    header += " (\n";
    header += "\t input\t\\clock\n";
    header += "\t,input\t\\reset\n";
    fprintf(output_file, "%s", header.c_str());
    header.clear();
    for (auto pp : ports) { pp->PrintVerilog(); }
    header += ");\n\n";
  }
  fprintf(output_file, "%s", header.c_str());
  header.clear();

  PrintVerilogWires();
  for (auto s : ssm) {
    s->PrintVerilogWires();
  }
  PrintVerilogVars();
  for (auto s : ssm) {
    s->PrintVerilogVars();
  }
  PrintVerilogParameters();
  for (auto s : ssm) {
    s->PrintVerilogParameters();
  }

  return;
}

void StateMachine::PrintScopeParam(StateMachine *p, std::string &str) {
  str = str + "SM" + std::to_string(p->GetNum()) + "_";
  if (p->par) { PrintScopeParam(p->GetPar(), str); }

  return;
}

void StateMachine::PrintScopeVar(StateMachine *p, std::string &str) {
  str = str + "sm" + std::to_string(p->GetNum()) + "_";
  if (p->par) { PrintScopeVar(p->GetPar(), str); }

  return;
}

void StateMachine::PrintVerilogState(std::vector<std::pair<State *, Condition *>> s, std::string &str) {
  for (auto ss : s) {
    if (ss.first->isPrinted()) { continue; }
    ss.first->PrintVerilog(str);
  }
  for (auto ss : s) {
    if (ss.first->isPrinted()) { continue; }
    PrintVerilogState(ss.first->GetNextState(), str);
  }
  return;
}

void State::PrintVerilog(std::string &str) {
  if (printed) { return; }
  printed = 1;
  for (auto c : ns) {
    c.first->PrintVerilog(str);
  }
  for (auto c : ns) {
    str += "else if (";
    c.second->PrintVerilogDeclRaw(str);
    str += ") begin\n\t";
    if (par) { PrintScopeVar(par, str); }
    str += "state <= ";
    if (par) { PrintScopeParam(par, str); }
    str = str + "STATE_" + std::to_string(c.first->GetNum()) + ";\nend ";
  }

  return;
}

void Data::PrintVerilogAssignment(std::string &str) {
  if (printed) { return; }
  printed = 1;
  str += "\t\\";
  act_boolean_netlist_t *bnl;
  bnl = BOOL->getBNL(scope->GetProc());
  act_dynamic_var_t *dv;
  dv = BOOL->isDynamicRef(bnl, id);

  char tmp[1024];
  if (!dv) { 
    id->sPrint(tmp,1024);
    str += tmp;
  } else {
    print_array_ref(id, scope, str);
  }
  str += " <= ";
  if (type == 0) {
    PrintExpression(u.assign.e, scope, str);
  } else if (type == 1) {
    str +=  "\\";
    u.recv.chan->sPrint(tmp,1024);
    str += tmp;
  } else if (type == 2) {
    PrintExpression(u.send.se, scope, str);
  }
  str += " ;\n";

  return;
}

//f = 0 - if
//f = 1 - else if
//f = 2 - else
//f = 3 - combinational else
void Data::PrintVerilog(int f, std::string &da)
{
  char tmp[1024];

  if (f == 0) { da += "if ("; }
  else if (f == 1) { da += "else if ( "; }
  else if (f == 3) { da += "else "; }
  if (f == 0 || f == 1) {
    if (type == 1) {
      u.recv.up_cond->PrintVerilogDeclRaw(da);
    } else if (type == 2) {
      u.send.up_cond->PrintVerilogDeclRaw(da);
    } else {
      cond->PrintVerilogDeclRaw(da);
    }
    da += ")";
    da += " begin\n";
    PrintVerilogAssignment(da);
    da += "end ";
  }
  if (f == 2) {
    da += "else begin\n";
    da += "\t\\";
    id->sPrint(tmp, 1024);
    da = da + tmp + " <= \\";
    id->sPrint(tmp, 1024);
    da = da + tmp + " ;\n";
    da += "end";
  }
  if (f == 3) {
    da += " begin\n";
    printed = 0;
    PrintVerilogAssignment(da);
    da += "end ";
  }

  return;
}

void StateMachine::PrintVerilogData()
{
  int ef = 0;
  std::string da;
  char tmp[1024]; 

  for (auto id : data) {
    act_boolean_netlist_t *bnl;
    bnl = BOOL->getBNL(p);
    act_dynamic_var_t *dv;
    dv = BOOL->isDynamicRef(bnl, id.first->toid());
    
    if (id.second[0]->GetType() == 2) {
      da += "always @(*)\n";
    } else {
      da += "always @(posedge \\clock )\n";
    }
  
    if (!dv) {
      id.first->toid()->sPrint(tmp,1024);
      da = da + "if (\\reset ) begin\n\t\\" + tmp + " <= 0;\nend ";
    } 
    for (auto dd : id.second) {
      if (!dv) {
        dd->PrintVerilog(1, da);
      } else {
        dd->PrintVerilog(ef, da);
        ef = 1;
      }
      if (dd == id.second[id.second.size()-1] && id.second[0]->GetType() != 2) {
        if (!dv) { dd->PrintVerilog(2, da); }
      } else if (dd == id.second[id.second.size()-1] && id.second[0]->GetType() == 2) {
        id.second[0]->PrintVerilog(3,da);
      }
    }
    ef = 0;

    fprintf(output_file, "%s\n\n", da.c_str());
    da.clear();
  }

  return;
}

void Data::PrintVerilogHSlhs(std::string &str) {

  Scope *s = act_scope->CurScope();
  act_connection *cid;
  
  char tmp[1024];
  
  if (type == 1) {
    cid = u.recv.chan->Canonical(s);
  } else if (type == 2 || type == 4) {
    cid = id->Canonical(s);
  }
  
  if (type == 1) {
    u.recv.chan->sPrint(tmp,1024);
  } else if (type == 2 || type == 4) {
    id->sPrint(tmp,1024);
  }
  str += tmp;
  if (scope->IsPort(cid) == 0) {
    str += "_valid ";
  } else if (scope->IsPort(cid) == 1) {
    str += "_valid ";
  } else if (scope->IsPort(cid) == 2) {
    str += "_ready ";
  } else {
    str += "Whaaaat?!" + std::to_string(scope->IsPort(cid)) + "\n";
  }

  return;
}

void Data::PrintVerilogHSrhs(std::string &str){

  if (type == 1) {
    u.recv.up_cond->PrintVerilogDeclRaw(str);
  } else if (type == 2) {
    u.send.up_cond->PrintVerilogDeclRaw(str);
  }

  return;
}

void StateMachine::PrintVerilogDataHS()
{
  std::string hs;

  for (auto id : hs_data) {
    if (id.second[0]->GetType() == 4) {
      hs += "always @(posedge clock)\nif (reset)\n\t\\";
      id.second[0]->PrintVerilogHSlhs(hs);
      hs += " <= 0";
    } else {
      hs += "always @(*)\n\t\\";
      id.second[0]->PrintVerilogHSlhs(hs);
      hs += " <= (";
      for (auto dd : id.second) {
        if (dd->GetType() == 1 || dd->GetType() == 2) {
          dd->PrintVerilogHSrhs(hs);
          if (dd != id.second[id.second.size()-1]) { hs += " | "; }
        }
      }
      hs += " ) & ~reset ";
    }
    hs += ";\n\n";
    fprintf(output_file, "%s", hs.c_str());
    hs.clear();
  }

  return;
}

void Port::PrintName(std::string &str){

  char tmp[1024];

  connection->toid()->sPrint(tmp,1024);
  str += tmp;

  return;
}

void StateMachineInst::PrintVerilog(){

  std::string inst = "\\";
  get_module_name(p, inst);
  inst = inst + " \\" + name->getName();
  if (array) {
    inst += array;
  }
  inst += " (\n";
  inst += "\t.\\clock (\\clock )\n\t,.\\reset (\\reset )\n";
  for (auto i = 0; i < ports.size(); i++) {
    if (ports[i]->GetChan() != 2) {
      inst += "\t,.\\";
      sm->GetPorts()[i]->PrintName(inst);
      inst += " (\\";
      ports[i]->PrintName(inst);
      inst += " )\n";
    }
    if (ports[i]->GetChan() != 0) {
      inst += "\t,.\\";
      sm->GetPorts()[i]->PrintName(inst);
      inst += "_ready (\\";
      ports[i]->PrintName(inst);
      inst += "_ready )\n";
      inst += "\t,.\\";
      sm->GetPorts()[i]->PrintName(inst);
      inst += "_valid (\\";
      ports[i]->PrintName(inst);
      inst += "_valid )\n";
    }
  }
  inst += ");\n\n";
  fprintf(output_file, "%s", inst.c_str());
  return;
}

void StateMachine::PrintVerilogBody()
{
  std::string sm;
  if (top) {
    sm += "always @(posedge \\clock )\n";
    sm += "if (\\reset ) begin\n\t";
    sm = sm + "sm" + std::to_string(number) + "_";
    if (par) { PrintScopeVar(par, sm); }
    sm += "state <= ";
    sm += "SM" + std::to_string(number) + "_";
    if (par) { PrintScopeParam(par, sm); }
    sm += "STATE_0;\nend ";
    top->PrintVerilog(sm);
    PrintVerilogState(top->GetNextState(), sm);
    sm += "else begin\n\tsm" + std::to_string(number) + "_";
    if (par) { PrintScopeVar(par,sm); }
    sm += "state <= sm"+ std::to_string(number) + "_";
    if (par) { PrintScopeVar(par,sm); }
    sm += "state;\nend\n";
  }
  fprintf(output_file, "%s\n", sm.c_str());
  sm.clear();

  return;
}

void StateMachine::PrintVerilog() {

  for (auto c : csm) {
    c->PrintVerilog();
  }

  for (auto sib : ssm) {
    sib->PrintVerilog();
  }

  std::string err;
  if (!top) {
    err += "/*\tNO CHP BODY IN THE PROCESS\t*/\n";
    fprintf(output_file, "%s", err.c_str());
  }
  

  std::string cond;
  for (auto cc : guard_condition) { cc->PrintVerilogExpr(cond); }
  for (auto cc : state_condition) { cc->PrintVerilogExpr(cond); }
  for (auto cc : commu_condition) { cc->PrintVerilogExpr(cond); }
  for (auto cc : comma_condition) { cc->PrintVerilogExpr(cond); }
  fprintf(output_file, "%s\n", cond.c_str());
  cond.clear();

  for (int ii = 0; ii < arb.size(); ii++) { arb[ii]->PrintInst(ii); }

  if (top) { PrintVerilogBody(); } 

  int has_comm = 0;
  int first = 0;
 
  int ef = 1;

  PrintVerilogData();
  PrintVerilogDataHS();
 
  for (auto i : inst) { i->PrintVerilog(); }
 
  if (!par & number == 0) {
    fprintf(output_file, "\n\nendmodule\n\n");
  }

  return; 
}

void CHPProject::PrintVerilog(Act *a, int sv, FILE *fout) {

  ActPass *apb = a->pass_find("booleanize");
  BOOL = dynamic_cast<ActBooleanizePass *>(apb);
  output_file = fout;
  for (auto n = hd; n; n = n->GetNext()) {
    if (!n->GetPar()) {
      n->PrintVerilogHeader(sv);
    }
    n->PrintVerilog();
  }

  return;  
}

void Arbiter::PrintInst(int n) {

  std::string arb;

  unsigned long size = log2(a.size());
  unsigned long total = pow(2,size);
printf("SIZE BEFORE: %li\n", size);
  if (total != a.size()) {
printf("HERE\n");
    size = size + 1;
    total = pow(2, size);
  }
printf("%li %li %ld\n", size, total, a.size());

  if (total > a.size()) { arb += "wire [" + std::to_string(total-a.size()-1) + ":0] placeholder_arb_" + std::to_string(n) + ";\n"; }

  arb += "rr #(\n\t.WIDTH(" + std::to_string(size) + ")\n";
  arb += ") arb_" + std::to_string(n) + " (\n";
  arb += "\t .\\clock (\\clock )\n\t,.\\reset (\\reset )\n\t,.req ({";

  for (auto i = 0; i < total - a.size(); i++) { arb += "1'b0,"; }
  for (auto i : a) {
    arb += "\\";
    i->PrintScopeVar(i->GetScope(), arb);
    arb = arb + "excl_guard_" + std::to_string(i->GetNum());
    if (i != a[a.size()-1]) {
      arb += " ,";
    }
  }
  arb += " })\n";

  arb += "\t,.grant ({";
  if (total > a.size()) { arb += "placeholder_arb_" + std::to_string(n) + ", "; }
  for (auto i : a) {
    arb += "\\";
    i->PrintScopeVar(i->GetScope(), arb);
    arb = arb + "guard_" + std::to_string(i->GetNum());
    if (i != a[a.size()-1]) {
      arb += " ,";
    }
  }
  arb += " })\n";

  arb += ");\n\n";
  fprintf(output_file, "%s", arb.c_str());
  arb.clear();

  return;
}

void Arbiter::PrintArbiter(FILE *fout){
std::string arb;
arb += "`timescale 1ns/1ps\n";
arb += "module rr\n";
arb += "#(\n";
arb += "\tparameter W = 4\n";
arb += ")(\n";
arb += "\t input\t\t\t\tclock\n";
arb += "\t,input\t\t\t\treset\n";
arb += "\t,input\t[2**W-1:0]\treq\n";
arb += "\t,output\t[2**W-1:0]\tgnt\n";
arb += ");\n";
arb += "\n";
arb += "wire\t[2**W-1:0]\ttfpe\t[W:0][1:0];\n";
arb += "\n";
arb += "wire\t\t\t\thp_gnt;\n";
arb += "\n";
arb += "reg\t\t[2**W-1:0]\tmask;\n";
arb += "\n";
arb += "wire\t[2**W-1:0]\tth_gnt;\n";
arb += "\n";
arb += "wire\t[2**W-1:0]\treq_masked;\n";
arb += "\n";
arb += "always @(posedge clock)\n";
arb += "if (reset) mask <= 0;\n";
arb += "else       mask <= th_gnt; \n";
arb += "\n";
arb += "assign req_masked = req & mask;\n";
arb += "\n";
arb += "genvar i, j, k;\n";
arb += "generate\n";
arb += "for (j = 0; j < 2; j = j + 1) begin\n";
arb += "\tfor (k = 0; k < W+1; k = k + 1) begin\n";
arb += "\t\tfor (i = 0; i < 2**W; i = i + 1) begin\n";
arb += "\n";
arb += "\t\tif (k == 0) begin\n";
arb += "\t\t\tif (j == 0)\n";
arb += "\t\t\t\tif ((i % 2) == 1)   assign tfpe[k][j][i] = req_masked[i];\n";
arb += "\t\t\t\telse                assign tfpe[k][j][i] = req_masked[i] | req_masked[i+1];\n";
arb += "\t\t\telse\n";
arb += "\t\t\t\tif ((i % 2) == 1)   assign tfpe[k][j][i] = req[i];\n";
arb += "\t\t\t\telse                assign tfpe[k][j][i] = req[i] | req[i+1];\n";
arb += "\t\tend\n";
arb += "\n";
arb += "\t\telse if (k == W) begin\n";
arb += "\t\t\tif (i == 0 | i == (2**W-1)) assign tfpe[k][j][i] = tfpe[k-1][j][i];\n";
arb += "\t\t\telse if ((i % 2) == 1)      assign tfpe[k][j][i] = tfpe[k-1][j][i] | tfpe[k-1][j][i+1];\n";
arb += "\t\t\telse                        assign tfpe[k][j][i] = tfpe[k-1][j][i];\n";
arb += "\t\tend\n";
arb += "\n";
arb += "\t\telse begin\n";
arb += "\t\t\tif (((i % 2) == 1) | ((i > (2**W-2**k-1)))) assign tfpe[k][j][i] =  tfpe[k-1][j][i];\n";
arb += "\t\t\telse                                        assign tfpe[k][j][i] =  tfpe[k-1][j][i] | tfpe[k-1][j][i+(2**k)];\n";
arb += "\t\tend\n";
arb += "\n";
arb += "\t\tend\n";
arb += "\tend\n";
arb += "end\n";
arb += "endgenerate\n";
arb += "\n";
arb += "assign hp_gnt = |tfpe[W][0];\n";
arb += "assign th_gnt = hp_gnt ? tfpe[W][0] : tfpe[W][1];\n";
arb += "\n";
arb += "wire [2**W:0] pre_gnt;\n";
arb += "assign pre_gnt = {1'b0, th_gnt} ^ {th_gnt, 1'b1};\n";
arb += "assign gnt = pre_gnt[2**W:1];\n";
arb += "\n";
arb += "endmodule\n\n";

fprintf(fout, "%s", arb.c_str());
arb.clear();
return;
}

}
