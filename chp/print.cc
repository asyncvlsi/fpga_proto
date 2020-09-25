#include <act/state_machine.h>
#include <act/act.h>

/*
    Since I have too many print functions I moved all of
    them to the separate file :)
 */

namespace fpga {

std::string get_module_name (Process *p) {

  const char *mn = p->getName();
  int len = strlen(mn);
  std::string buf;

  for (auto i = 0; i < len; i++) {
    if (mn[i] == 0x3c) {
      continue;
    } else if (mn[i] == 0x3e) {
      continue;
    } else {
      buf += mn[i];
    }
  }

  return buf;

}



/*
 *  CHP Project Class
 */

void CHPProject::PrintPlain() {
  for (auto n = hd; n; n = n->GetNext()) {
    n->PrintPlain();
  }
}

void CHPProject::PrintVerilog() {
  for (auto n = hd; n; n = n->GetNext()) {
    if (!n->GetPar()) {
      n->PrintVerilogHeader();
    }
    n->PrintVerilog();
  }
}

/*
 *  State-Machine Class
 */

void StateMachine::PrintPlainState(std::vector<std::pair<State *, Condition *>> s) {
  for (auto ss : s) {
    if (ss.first->isPrinted()) { continue; }
    ss.first->PrintPlain(0);
  }
  for (auto ss : s) {
    if (ss.first->isPrinted()) { continue; }
    PrintPlainState(ss.first->GetNextState());
  }
}

void StateMachine::PrintPlain() {

  for (auto c : csm) {
    c->PrintPlain();
    fprintf(stdout, "===============\n");
  }

  for (auto cc : guard_condition) {
    cc->PrintPlain();
  }
  for (auto cc : state_condition) {
    cc->PrintPlain();
  }
  for (auto cc : commu_condition) {
    cc->PrintPlain();
  }
  for (auto cc : comma_condition) {
    cc->PrintPlain();
  }
  fprintf(stdout, "\n");


  top->PrintPlain();
  PrintPlainState(top->GetNextState());

}

void StateMachine::PrintVerilogVars() {
  for (auto v : vars) {
    v->PrintVerilog(); 
  }
  fprintf(stdout, "\n");
}

void StateMachine::PrintVerilogWires(){
  for (auto c : csm) {
    c->PrintVerilogWires();
  }

  for (auto cc : guard_condition) {
    fprintf(stdout, "wire ");
    cc->PrintVerilog(0);
    fprintf(stdout, ";\n");
  }
  for (auto cc : state_condition) {
    fprintf(stdout, "wire ");
    cc->PrintVerilog(0);
    fprintf(stdout, ";\n");
  }
  for (auto cc : commu_condition) {
    fprintf(stdout, "wire ");
    cc->PrintVerilog(0);
    fprintf(stdout, ";\n");
  }
  for (auto cc : comma_condition) {
    fprintf(stdout, "wire ");
    cc->PrintVerilog(0);
    fprintf(stdout, ";\n");
  }
  fprintf(stdout, "\n");
}

void StateMachine::PrintVerilogHeader() {
  if (p) {
    fprintf(stdout, "`timescale 1ns/1ps\n\n");
    std::string name = get_module_name(p);
    fprintf(stdout, "module \\%s (\n", name.c_str());
    fprintf(stdout, "\t input\tclock\n");
    fprintf(stdout, "\t,input\treset\n");
    for (auto pp : ports) {
      pp->Print();
      fprintf(stdout, "\n");
    }
    fprintf(stdout, ");\n\n");
  }

  PrintVerilogWires();
  PrintVerilogVars();
  PrintVerilogParameters();
  fprintf(stdout, "\n");

}

void StateMachine::PrintVerilogState(std::vector<std::pair<State *, Condition *>> s) {
  for (auto ss : s) {
    if (ss.first->isPrinted()) { continue; }
    ss.first->PrintVerilog(0);
  }
  for (auto ss : s) {
    if (ss.first->isPrinted()) { continue; }
    PrintVerilogState(ss.first->GetNextState());
  }
}

void StateMachine::PrintParent(StateMachine *p) {
  fprintf(stdout, "sm%i_", p->GetNum());
  if (p->par) {
    PrintParent(p->GetPar());
  }
}

void StateMachine::PrintVerilogParameters(){
  fprintf(stdout, "reg [31:0] ");
  top->PrintVerilogName(1);
  fprintf(stdout, " = 0;\n");
  for (auto i = 0; i < size; i++) {
    fprintf(stdout, "localparam ");
    top->PrintVerilogName(1);
    fprintf(stdout, "_%i = %i;\n", i, i);
  }
  fprintf(stdout, "\n");
  for (auto c : csm) {
    c->PrintVerilogParameters();
  }
}

void StateMachine::PrintVerilog() {
  for (auto c : csm) {
    c->PrintVerilog();
    fprintf(stdout, "\n");
  }

  fprintf(stdout, "/*\n\tState Machine #%i\n*/\n\n", number);

  for (auto cc : guard_condition) {
    fprintf(stdout, "assign ");
    cc->PrintVerilog(1);
    fprintf(stdout, ";\n");
  }
  for (auto cc : state_condition) {
    fprintf(stdout, "assign ");
    cc->PrintVerilog(1);
    fprintf(stdout, ";\n");
  }
  for (auto cc : commu_condition) {
    fprintf(stdout, "assign ");
    cc->PrintVerilog(1);
    fprintf(stdout, ";\n");
  }
  for (auto cc : comma_condition) {
    fprintf(stdout, "assign ");
    cc->PrintVerilog(1);
    fprintf(stdout, ";\n");
  }
  fprintf(stdout, "\n");

  fprintf(stdout, "always @(posedge clock)\n");
  fprintf(stdout, "if (reset)\n\t");
  fprintf(stdout, "sm%i_", number);
  if (par) {
    PrintParent(par);
  }
  fprintf(stdout, "state <= ");
  fprintf(stdout, "sm%i_", number);
  if (par) {
    PrintParent(par);
  }
  fprintf(stdout, "state_0;\n");
  
  top->PrintVerilog();
  PrintVerilogState(top->GetNextState());

  fprintf(stdout, "\n");

  int has_comm = 0;
  int first = 0;

  for (auto id : data) {
    fprintf(stdout, "always @(posedge clock)\n");
    fprintf(stdout, "if (reset) begin\n\t");
    fprintf(stdout, "%s", std::get<2>(id.first).c_str());
    fprintf(stdout, "[%i:%i]", std::get<0>(id.first), std::get<1>(id.first));
    fprintf(stdout, " <= 0;\n");
    fprintf(stdout, "end\n");
    for (auto dd : id.second) {
      dd->PrintVerilogCondition();
      fprintf(stdout, " begin\n\t");
      dd->PrintVerilogAssignment();
      fprintf(stdout, "end\n");
      if (dd->GetType() == 1 || dd->GetType() == 2) {
        has_comm = 1;
      }
    }
    fprintf(stdout, "\n");

    if (has_comm) {
      for (auto dd : id.second) {
        if (dd->GetType() == 1 || dd->GetType() == 2) {
          if (first == 0) {
            dd->PrintVerilogHS(first);
          }
          first = 1;
          dd->PrintVerilogCondition();
          dd->PrintVerilogHS(first);
          first = 2;
          dd->PrintVerilogConditionUP();
          dd->PrintVerilogHS(first);
          first = 1;
        }
      }
    }
    first = 0;
    fprintf(stdout, "\n");
  }

  if (p) {
    fprintf(stdout, "\n\nendmodule\n\n");
  }
 
}

/*
 *  State Class
 */

void State::PrintParent(StateMachine *p) {
  fprintf(stdout, "sm%i_", p->GetNum());
  if (p->GetPar()) {
    PrintParent(p->GetPar());
  }
}

void State::PrintPlain(int p) {
  if (p) {  printed = 1; }
  if (type == ACT_CHP_SEMI) {
    fprintf(stdout, "SEMI\n");
  } else if (type == ACT_CHP_COMMA) {
    fprintf(stdout, "COMMA\n");
  } else if (type == ACT_CHP_SELECT) {
    fprintf(stdout, "SELECT\n");
  } else if (type == ACT_CHP_SELECT_NONDET) {
    fprintf(stdout, "NONDET\n");
  } else if (type == ACT_CHP_LOOP) {
    fprintf(stdout, "LOOP\n");
  } else if (type == ACT_CHP_SKIP) {
    fprintf(stdout, "SKIP\n");
  } else if (type == ACT_CHP_ASSIGN) {
    fprintf(stdout, "ASSIGN\n");
  } else if (type == ACT_CHP_SEND) {
    fprintf(stdout, "SEND\n");
  } else if (type == ACT_CHP_RECV) {
    fprintf(stdout, "RECV\n");
  } else {
    fprintf(stdout, "Not supported type: %i\n", type);
  }
  for (auto c : ns) {
    if (par) PrintParent(par);
    fprintf(stdout, "state%i->", number);
    if (par) PrintParent(par);
    fprintf(stdout, "state%i \n", c.first->GetNum());
    fprintf(stdout, " (");
    c.second->PrintPlain();
    fprintf(stdout, " )\n");
  }
}

void State::PrintType(){
  if (type == ACT_CHP_SEMI) {
    fprintf(stdout, "SEMI\n");
  } else if (type == ACT_CHP_COMMA) {
    fprintf(stdout, "COMMA\n");
  } else if (type == ACT_CHP_SELECT) {
    fprintf(stdout, "SELECT\n");
  } else if (type == ACT_CHP_SELECT_NONDET) {
    fprintf(stdout, "NONDET\n");
  } else if (type == ACT_CHP_LOOP) {
    fprintf(stdout, "LOOP\n");
  } else if (type == ACT_CHP_SKIP) {
    fprintf(stdout, "SKIP\n");
  } else if (type == ACT_CHP_ASSIGN) {
    fprintf(stdout, "ASSIGN\n");
  } else if (type == ACT_CHP_SEND) {
    fprintf(stdout, "SEND\n");
  } else if (type == ACT_CHP_RECV) {
    fprintf(stdout, "RECV\n");
  } else {
    fprintf(stdout, "Not supported type: %i\n", type);
  }
}

void State::PrintVerilogName(int f = 0) {
  if (par) PrintParent(par);
  if (f == 0) {
    fprintf(stdout, "state_%i", number);
  } else {
    fprintf(stdout, "state");
  }
}

void State::PrintVerilog(int p) {
  if (printed) { return; }
  printed = 1;
  for (auto c : ns) {
    fprintf(stdout, "else if (");
    c.second->PrintVerilog(0);
    fprintf(stdout, ")\n\t");
    if (par) PrintParent(par);
    fprintf(stdout, "state <= ");
    if (par) PrintParent(par);
    fprintf(stdout, "state_%i", c.first->GetNum());
    fprintf(stdout, ";\n");
  for (auto c : ns) {
    c.first->PrintVerilog();
  }
  }
}

/*
 *  Condition Class
 */

void PrintExpression(Expr *e) {
  switch (e->type) {
    case (E_AND):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " & ");
      PrintExpression(e->u.e.r);
      break;
    case (E_OR):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " | ");
      PrintExpression(e->u.e.r);
      break;
    case (E_NOT):
      fprintf(stdout, " ~");
      PrintExpression(e->u.e.l);
      break;
    case (E_PLUS):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " + ");
      PrintExpression(e->u.e.r);
      break;
    case (E_MINUS):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " - ");
      PrintExpression(e->u.e.r);
      break;
    case (E_MULT):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " * ");
      PrintExpression(e->u.e.r);
      break;
    case (E_DIV):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " / ");
      PrintExpression(e->u.e.r);
      break;
    case (E_MOD):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " % ");
      PrintExpression(e->u.e.r);
      break;
    case (E_LSL):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " << ");
      PrintExpression(e->u.e.r);
      break;
    case (E_LSR):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " >> ");
      PrintExpression(e->u.e.r);
      break;
    case (E_ASR):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " >>> ");
      PrintExpression(e->u.e.r);
      break;
    case (E_UMINUS):
      fprintf(stdout, " -");
      PrintExpression(e->u.e.l);
      break;
    case (E_INT):
      fprintf(stdout, "%i", e->u.v);
      break;
    case (E_VAR):
      ActId *id;
      id = (ActId *)e->u.e.l;
      id->Print(stdout);
      break;
    case (E_QUERY):
      break;
    case (E_LPAR):
      break;
    case (E_RPAR):
      break;
    case (E_XOR):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " ^ ");
      PrintExpression(e->u.e.r);
      break;
    case (E_LT):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " < ");
      PrintExpression(e->u.e.r);
      break;
    case (E_GT):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " > ");
      PrintExpression(e->u.e.r);
      break;
    case (E_LE):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " <=");
      PrintExpression(e->u.e.r);
      break;
    case (E_GE):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " >= ");
      PrintExpression(e->u.e.r);
      break;
    case (E_EQ):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " == ");
      PrintExpression(e->u.e.r);
      break;
    case (E_NE):
      PrintExpression(e->u.e.l);
      fprintf(stdout, " != ");
      PrintExpression(e->u.e.r);
      break;
    case (E_TRUE):
      fprintf(stdout, " 1'b1 ");
      break;
    case (E_FALSE):
      fprintf(stdout, " 1'b0 ");
      break;
    case (E_COLON):
      fprintf(stdout, " : ");
      break;
    case (E_PROBE):
      fprintf(stdout, "PROBE");
      break;
    case (E_COMMA):
      fprintf(stdout, "COMMA");
      break;
    case (E_CONCAT):
      fprintf(stdout, "CONCAT");
      PrintExpression(e->u.e.l);
      fprintf(stdout, " ");
      PrintExpression(e->u.e.r);
      break;
    case (E_BITFIELD):
      fprintf(stdout, "BITFIELD");
      fprintf(stdout, "[");
      PrintExpression(e->u.e.l);
      fprintf(stdout, " ");
      PrintExpression(e->u.e.r);
      fprintf(stdout, "]");
      break;
    case (E_COMPLEMENT):
      fprintf(stdout, " ~");
      PrintExpression(e->u.e.l);
      break;
    case (E_REAL):
      fprintf(stdout, "%i", e->u.v);
      break;
    case (E_RAWFREE):
      break;
    case (E_END):
      break;
    case (E_NUMBER):
      break;
    case (E_FUNCTION):
      break;
    default:
      fprintf(stdout, "Whaaat?! %i\n", e->type);
      break;
  }

}

void Condition::PrintExpr(Expr *e) {
  PrintExpression(e);
}

void Condition::PrintScope(StateMachine *sc){
  fprintf(stdout, "sm%i_", sc->GetNum());
  if (sc->GetPar()) {
    PrintScope(sc->GetPar());
  }
}

void Condition::PrintPlain() {
  fprintf(stdout, "Condition number : %i\n", num);
  StateMachine *sm;
  switch (type) {
    case (0) :
      PrintScope(scope);
      fprintf(stdout, "commu_compl = ");
      u.v->Print(stdout);
      fprintf(stdout, "_valid & ");
      u.v->Print(stdout);
      fprintf(stdout, "_ready");
      break;
    case (1) :
      PrintScope(scope);
      fprintf(stdout, "guard = ");
      PrintExpr(u.e);
      break;
    case (2) :
      sm = u.s->GetPar();
      PrintScope(scope);
      fprintf(stdout, "state_cond_%i = ", num);
      PrintScope(sm);
      fprintf(stdout, "state == "); 
      PrintScope(sm);
      fprintf(stdout, "state_%i", u.s->GetNum());
      break;
    case (3) :
      PrintScope(scope);
      fprintf(stdout, "cond%i = ", num);
      for (auto cc : u.c->c) {
        if (u.c->type == 2) {
          fprintf(stdout, "!");
        }
        PrintScope(cc->GetScope());
        if (cc->GetType() == 0) {
          fprintf(stdout, "commun_compl_%i ", cc->GetNum());
        } else if (cc->GetType() == 1) {
          fprintf(stdout, "guard_%i ", cc->GetNum());
        } else if (cc->GetType() == 2) {
          State *ss = cc->GetState();
          sm = ss->GetPar();
          fprintf(stdout, "state_cond_%i ", cc->GetNum());
        } else {
          fprintf(stdout, "cond%i ", cc->GetNum());
        }
        if (cc != u.c->c[u.c->c.size()-1]) {
          if (u.c->type != 1) {
            fprintf(stdout," & ");
          } else {
            fprintf(stdout," | ");
          }
        }
      }
      break;
    default:
      fatal_error("Wrong condition type\n");
  }
  fprintf(stdout,"\n");
}

void Condition::PrintVerilog(int f){
  StateMachine *sm;
  if (f == 0) {
    switch (type) {
      case (0) :
        PrintScope(scope);
        fprintf(stdout, "commu_compl_%i", num);
        break;
      case (1) :
        PrintScope(scope);
        fprintf(stdout, "guard_%i", num);
        break;
      case (2) :
        PrintScope(scope);
        fprintf(stdout, "state_cond_%i", num);
        break;
      case (3) :
        PrintScope(scope);
        fprintf(stdout, "cond_%i", num);
        break;
      default :
        fatal_error("!!!\n");
    }
  } else if (f == 1) {
     switch (type) {
      case (0) :
        PrintScope(scope);
        fprintf(stdout, "commu_compl_%i = ", num);
        u.v->Print(stdout);
        fprintf(stdout, "_valid & ");
        u.v->Print(stdout);
        fprintf(stdout, "_ready");
        break;
      case (1) :
        PrintScope(scope);
        fprintf(stdout, "guard_%i = ", num);
        PrintExpr(u.e);
        fprintf(stdout, " ? 1'b1 : 1'b0 ");
        break;
      case (2) :
        sm = u.s->GetPar();
        PrintScope(scope);
        fprintf(stdout, "state_cond_%i = ", num);
        PrintScope(sm);
        fprintf(stdout, "state == "); 
        PrintScope(sm);
        fprintf(stdout, "state_%i", u.s->GetNum());
        break;
      case (3) :
        PrintScope(scope);
        fprintf(stdout, "cond_%i = ", num);
      for (auto cc : u.c->c) {
        if (u.c->type == 2) {
          fprintf(stdout, "!");
        }
        PrintScope(cc->GetScope());
        if (cc->GetType() == 0) {
          fprintf(stdout, "commu_compl_%i", cc->GetNum());
        } else if (cc->GetType() == 1) {
          fprintf(stdout, "guard_%i", cc->GetNum());
        } else if (cc->GetType() == 2) {
          State *ss = cc->GetState();
          sm = ss->GetPar();
          fprintf(stdout, "state_cond_%i", cc->GetNum());
        } else {
          fprintf(stdout, "cond_%i", cc->GetNum());
        }
        if (cc != u.c->c[u.c->c.size()-1]) {
          if (u.c->type != 1) {
            fprintf(stdout," & ");
          } else {
            fprintf(stdout," | ");
          }
        }
      }
        break;
      default :
        fatal_error("!!!\n");
    }
  }
}

/*
 *  Data
 */
void Data::PrintVerilogHS(int f){
  Scope *s = act_scope->CurScope();
  act_connection *cid;

  if (type == 1) {
    cid = u.recv.chan->Canonical(s);
  } else if (type == 2) {
    cid = id->Canonical(s);
  }
 

  if (f == 0) {
    fprintf(stdout, "always @(posedge clock)\n");
    fprintf(stdout, "if (reset) begin\n\t");
    if (type == 1) {
      u.recv.chan->Print(stdout);
    } else if (type == 2) {
      id->Print(stdout);
    }
    if (scope->IsPort(cid) == 1) {
      fprintf(stdout, "_valid <= 1'b0;\n");
    } else if (scope->IsPort(cid) == 2) {
      fprintf(stdout, "_ready <= 1'b0;\n");
    } else {
      fprintf(stdout, "Whaaaat?!\n");
    }
    fprintf(stdout, "end\n");
  } else {
    fprintf(stdout, " begin\n\t");
    if (type == 1) {
      u.recv.chan->Print(stdout);
    } else if (type == 2) {
      id->Print(stdout);
    }
    if (f == 1) {
      if (scope->IsPort(cid) == 1) {
        fprintf(stdout, "_valid <= 1'b0;\n");
      } else if (scope->IsPort(cid) == 2) {
        fprintf(stdout, "_ready <= 1'b0;\n");
      } else {
        fprintf(stdout, "Whaaaat?!\n");
      }
      fprintf(stdout, "end\n");
    } else if (f == 2) {
      if (scope->IsPort(cid) == 1) {
        fprintf(stdout, "_valid <= 1'b1;\n");
      } else if (scope->IsPort(cid) == 2) {
        fprintf(stdout, "_ready <= 1'b1;\n");
      } else {
        fprintf(stdout, "Whaaaat?!\n");
      }
      fprintf(stdout, "end\n");
    }
  }
}

void Data::PrintVerilogVar() {
  id->Print(stdout);
}

void Data::PrintVerilogAssignment() {
  if (printed) {return;}
  printed = 1;
  if (type == 0) {
    id->Print(stdout);
    fprintf(stdout, " <= ");
    PrintExpression(u.assign.e);
    fprintf(stdout, " ;\n");
  } else if (type == 1) {
    id->Print(stdout);
    fprintf(stdout, " <= ");
    u.recv.chan->Print(stdout);
    fprintf(stdout, "[%i:%i]", up,dn);
    fprintf(stdout, " ;\n");
  } else if (type == 2) {
    id->Print(stdout);
    fprintf(stdout, "[%i:%i]", up,dn);
    fprintf(stdout, " <= ");
    PrintExpression(u.send.se);
    fprintf(stdout, " ;\n");
  }
 
}

void Data::PrintVerilogCondition(){
  fprintf(stdout, "else if (");
  cond->PrintVerilog(0);
  fprintf(stdout, ")");
}

void Data::PrintVerilogConditionUP(){
  fprintf(stdout, "else if (");
  if (type == 1) {
    u.recv.up_cond->PrintVerilog(0);
  } else if (type == 2) {
    u.send.up_cond->PrintVerilog(0);
  }
  fprintf(stdout, ")");
}

/*
 *  Variable class
 */
void Variable::PrintVerilog (){
  if (type == 0) {
    fprintf(stdout, "reg\t");
  } else {
    fprintf(stdout, "wire\t");
  }
  fprintf(stdout, "[%i:0]\t", width);
  id->toid()->Print(stdout);
  fprintf(stdout, " ;\n");
}
}
