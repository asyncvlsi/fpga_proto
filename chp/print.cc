#include <act/state_machine.h>
#include <act/act.h>
#include <string>

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
			buf += "_";
      continue;
    } else if (mn[i] == 0x3e) {
			buf += "_";
      continue;
    } else if (mn[i] == 0x2c) {
			buf += "_";
      continue;
    } else {
      buf += mn[i];
    }
  }

  return buf;

}

std::string print_array_ref (ActId *id) {

	char buf[1024];

	id->sPrint(buf, 1024);

	std::string ret_id;

	int once = 0;

	for (auto i = 0; i < 1024; i++) {
		if (buf[i] == 0x5B && once == 0) {
			ret_id += 0x20;
			ret_id += buf[i];
			once = 1;
		} else if (buf[i] == 0x20) {
			continue;
		} else {
			ret_id += buf[i];
		}
	}

	return ret_id;

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
		if (v->IsPort() == 0) {
			v->PrintVerilog();
		}
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
		if (cc->GetType() == 4) {
			fprintf(stdout, "wire ");
			cc->PrintVerilog(2);
			fprintf(stdout, ";\n");
		}
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
    fprintf(stdout, "\t input\t\\clock\n");
    fprintf(stdout, "\t,input\t\\reset\n");
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
    ss.first->PrintVerilog(2);
  }
  for (auto ss : s) {
    if (ss.first->isPrinted()) { continue; }
    PrintVerilogState(ss.first->GetNextState());
  }
}

void StateMachine::PrintParent(StateMachine *p, int f = 0) {
	if (f == 0) {
	  fprintf(stdout, "sm%i_", p->GetNum());
	} else if (f == 2) {
	  fprintf(stdout, "SM%i_", p->GetNum());
	}
  if (p->par) {
    PrintParent(p->GetPar(), f);
  }
}

void StateMachine::PrintVerilogParameters(){
	if (top) {
  	fprintf(stdout, "reg [31:0] ");
  	top->PrintVerilogName(1);
  	fprintf(stdout, " = 0;\n");
  	for (auto i = 0; i < size; i++) {
  	  fprintf(stdout, "localparam ");
  	  top->PrintVerilogName(2);
  	  fprintf(stdout, "_%i = %i;\n", i, i);
  	}
  	fprintf(stdout, "\n");
  	for (auto c : csm) {
  	  c->PrintVerilogParameters();
  	}
	}
}

void StateMachine::PrintVerilog() {
  for (auto c : csm) {
    c->PrintVerilog();
    fprintf(stdout, "\n");
  }

	if (top) {
	  fprintf(stdout, "/*\n\tState Machine type:");
		top->PrintType();
  	fprintf(stdout, "*/\n\n");
	} else {
		fprintf(stdout, "/*\tNO CHP BODY IN THE PROCESS\t*/\n");
	}

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

	for (int ii = 0; ii < arb.size(); ii++) {
		arb[ii]->PrintInst(ii);
	}

	if (top) {
  	fprintf(stdout, "always @(posedge \\clock )\n");
  	fprintf(stdout, "if (\\reset )\n\t");
  	fprintf(stdout, "sm%i_", number);
  	if (par) {
  	  PrintParent(par);
  	}
  	fprintf(stdout, "state <= ");
  	fprintf(stdout, "SM%i_", number);
  	if (par) {
  	  PrintParent(par, 2);
  	}
  	fprintf(stdout, "STATE_0;\n");
	  top->PrintVerilog();
	  PrintVerilogState(top->GetNextState());
	}

  fprintf(stdout, "\n");

  int has_comm = 0;
  int first = 0;

	int ef = 1;

  for (auto id : data) {
    fprintf(stdout, "always @(posedge \\clock )\n");
   	fprintf(stdout, "if (\\reset ) begin\n");
		fprintf(stdout, "\t\\");
		id.first->toid()->Print(stdout);
  	fprintf(stdout, " <= 0;\n");
   	fprintf(stdout, "end\n");
	 	ef = 0;
    for (auto dd : id.second) {
			if (dd->GetType() != 2) {
	      dd->PrintVerilogCondition(ef);
			} else {
				dd->PrintVerilogConditionUP(ef);
			}
      fprintf(stdout, " begin\n");
      dd->PrintVerilogAssignment();
      fprintf(stdout, "end\n");
			ef = 0;
    }
		ef = 1;
    fprintf(stdout, "\n");
  }

  for (auto id : hs_data) {
		first = 0;
		ef = 0;
  	for (auto dd : id.second) {
  	  if (dd->GetType() == 1 || dd->GetType() == 2) {
  	    if (first == 0) {
  	      dd->PrintVerilogHS(first);
  	      first = 1;
  	    }
  	    dd->PrintVerilogCondition(ef);
  	    dd->PrintVerilogHS(first);
  	    first = 2;
  	    dd->PrintVerilogConditionUP(ef);
  	    dd->PrintVerilogHS(first);
  	    first = 1;
  	  }
  	}
		fprintf(stdout, "\n");
	}


	for (auto i : inst) {
		i->PrintVerilog();
	}

  if (!par) {
    fprintf(stdout, "\n\nendmodule\n\n");
  }
 
}
/*
 *	State Machine Instance
 */
void StateMachineInst::PrintVerilog(){
	std::string mn = get_module_name(p);
	if (array) {
		fprintf(stdout, "\\%s \\%s%s (\n", mn.c_str(), name->getName(), array);
	} else {
		fprintf(stdout, "\\%s \\%s (\n", mn.c_str(), name->getName());
	}
	fprintf(stdout, "\t.\\clock (\\clock )\n");
	fprintf(stdout, "\t,.\\reset (\\reset )\n");
	for (auto i = 0; i < ports.size(); i++) {
		if (ports[i]->GetChan() != 2) {
			fprintf(stdout, "\t,.\\");
			sm->GetPorts()[i]->PrintName();
			fprintf(stdout, " (\\");
			ports[i]->PrintName();
			fprintf(stdout, " )\n");
		}
		if (ports[i]->GetChan() != 0) {
			fprintf(stdout, "\t,.\\");
			sm->GetPorts()[i]->PrintName();
			fprintf(stdout, "_ready (\\");
			ports[i]->PrintName();
			fprintf(stdout, "_ready )\n");
			fprintf(stdout, "\t,.\\");
			sm->GetPorts()[i]->PrintName();
			fprintf(stdout, "_valid (\\");
			ports[i]->PrintName();
			fprintf(stdout, "_valid )\n");

		}
	}
	fprintf(stdout, ");\n\n");
}


/*
 *  State Class
 */

void State::PrintParent(StateMachine *p, int f = 0) {
	if (f == 0) {
	  fprintf(stdout, "sm%i_", p->GetNum());
	} else if (f == 1) {
		fprintf(stdout, "SM%i_", p->GetNum());
	}
  if (p->GetPar()) {
    PrintParent(p->GetPar(), f);
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
  if (par) {
		if (f == 2) {
			PrintParent(par, 1);
		} else {
			PrintParent(par, 0);
		}
	}
  if (f == 0) {
    fprintf(stdout, "state_%i", number);
  } else if (f == 1) {
    fprintf(stdout, "state");
  } else if (f == 2) {
		fprintf(stdout, "STATE");
	}
}

void State::PrintVerilog(int p) {
  if (printed) { return; }
  printed = 1;
  for (auto c : ns) {
    c.first->PrintVerilog();
  }
  for (auto c : ns) {
    fprintf(stdout, "else if (");
    c.second->PrintVerilog(0);
    fprintf(stdout, ")\n\t");
    if (par) PrintParent(par);
    fprintf(stdout, "state <= ");
    if (par) PrintParent(par,1);
    fprintf(stdout, "STATE_%i", c.first->GetNum());
    fprintf(stdout, ";\n");
	}

}

/*
 *  Condition Class
 */

void PrintExpression(Expr *e, StateMachine *scope) {
//	fprintf(stdout, "TYPE: %i\n", e->type);
  switch (e->type) {
    case (E_AND):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " & ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_OR):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " | ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_NOT):
      fprintf(stdout, " ~");
      PrintExpression(e->u.e.l, scope);
      break;
    case (E_PLUS):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " + ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_MINUS):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " - ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_MULT):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " * ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_DIV):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " / ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_MOD):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " %% ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_LSL):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " << ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_LSR):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " >> ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_ASR):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " >>> ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_UMINUS):
      fprintf(stdout, " -");
      PrintExpression(e->u.e.l, scope);
      break;
    case (E_INT):
      fprintf(stdout, "32'd%lu", e->u.v);
      break;
    case (E_VAR):
      ActId *id;
      id = (ActId *)e->u.e.l;
			fprintf(stdout, "\\");
			id->Print(stdout);
      break;
    case (E_QUERY):
			PrintExpression(e->u.e.l, scope);
			fprintf(stdout, " ? ");
			PrintExpression(e->u.e.r->u.e.l, scope);
			fprintf(stdout, " : ");
			PrintExpression(e->u.e.r->u.e.r, scope);
      break;
    case (E_LPAR):
			fprintf(stdout, "LPAR\n");
      break;
    case (E_RPAR):
			fprintf(stdout, "RPAR\n");
      break;
    case (E_XOR):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " ^ ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_LT):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " < ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_GT):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " > ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_LE):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " <=");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_GE):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " >= ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_EQ):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " == ");
      PrintExpression(e->u.e.r, scope);
      break;
    case (E_NE):
      PrintExpression(e->u.e.l, scope);
      fprintf(stdout, " != ");
      PrintExpression(e->u.e.r, scope);
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
			Scope *act_scope;
			act_scope = scope->GetProc()->CurScope();
			id = (ActId *)e->u.e.l;
			act_connection *cc;
			cc = id->Canonical(act_scope);
			StateMachine *tmp;
			tmp = scope->GetPar();
			while (tmp->GetPar()) {tmp = tmp->GetPar(); }
			fprintf(stdout, "\\");
			id->Print(stdout);
			if (tmp->IsPort(cc) == 1) {
				fprintf(stdout,"_ready");
			} else if (tmp->IsPort(cc) == 2){
				fprintf(stdout,"_valid");
			}
      break;
    case (E_COMMA):
      fprintf(stdout, "COMMA");
      break;
    case (E_CONCAT):
      PrintExpression(e->u.e.l, scope);
			if (e->u.e.r) {
	      fprintf(stdout, " ,");
  	    PrintExpression(e->u.e.r, scope);
			} else {
				fprintf(stdout, " }");
			}
      break;
    case (E_BITFIELD):
			unsigned int l;
			unsigned int r;
			l = e->u.e.r->u.e.r->u.v;
			if (e->u.e.r->u.e.l) {
				r = e->u.e.r->u.e.l->u.v;
      } else {
				r = l;
			}
			fprintf(stdout, "\\");
			((ActId *)e->u.e.l)->Print(stdout);
      fprintf(stdout, " [");
			if (l!=r) {
				fprintf(stdout, "%i:", l);
				fprintf(stdout, "%i", r);
			} else {
				fprintf(stdout, "%i", r);
			}
      fprintf(stdout, "]");
      break;
    case (E_COMPLEMENT):
      fprintf(stdout, " ~");
      PrintExpression(e->u.e.l, scope);
      break;
    case (E_REAL):
      fprintf(stdout, "%lu", e->u.v);
      break;
		case (E_ANDLOOP):
			fprintf(stdout, "ANDLOOP you mean?\n");
			break;
		case (E_ORLOOP):
			fprintf(stdout, "ORLOOP you mean?\n");
			break;
		case (E_BUILTIN_INT):
			PrintExpression(e->u.e.l, scope);
			break;
		case (E_BUILTIN_BOOL):
			PrintExpression(e->u.e.l, scope);
			break;
    case (E_RAWFREE):
			fprintf(stdout, "RAWFREE\n");
      break;
    case (E_END):
			fprintf(stdout, "END\n");
      break;
    case (E_NUMBER):
			fprintf(stdout, "NUMBER\n");
      break;
    case (E_FUNCTION):
			fprintf(stdout, "FUNCTION\n");
      break;
    default:
      fprintf(stdout, "Whaaat?! %i\n", e->type);
      break;
  }

}

void Condition::PrintExpr(Expr *e) {
  PrintExpression(e, scope);
}

void Condition::PrintScope(StateMachine *sc, int f = 0){
	if (f == 0) {
	  fprintf(stdout, "sm%i_", sc->GetNum());
	} else if (f == 1) {
	  fprintf(stdout, "SM%i_", sc->GetNum());
	}
  if (sc->GetPar()) {
    PrintScope(sc->GetPar(), f);
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
      case (4) :
        PrintScope(scope);
        fprintf(stdout, "excl_guard_%i", num);
        break;
      default :
        fatal_error("!!!\n");
    }
  } else if (f == 1) {
    switch (type) {
      case (0) :
        PrintScope(scope);
        fprintf(stdout, "commu_compl_%i = \\", num);
        u.v->Print(stdout);
        fprintf(stdout, "_valid & \\");
        u.v->Print(stdout);
        fprintf(stdout, "_ready ");
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
        PrintScope(sm,1);
        fprintf(stdout, "STATE_%i", u.s->GetNum());
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
      case (4) :
        PrintScope(scope);
        fprintf(stdout, "excl_guard_%i = ", num);
        PrintExpr(u.e);
        fprintf(stdout, " ? 1'b1 : 1'b0 ");
        break;
      default :
        fatal_error("!!!\n");
    }
  } else if (f == 2) {
		PrintScope(scope);
		fprintf(stdout, "guard_%i", num);
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
    fprintf(stdout, "always @(posedge \\clock )\n");
    fprintf(stdout, "if (\\reset ) begin\n\t\\");
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
    fprintf(stdout, " begin\n\t\\");
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
	fprintf(stdout, "\t\\");
	id->Print(stdout);
	fprintf(stdout, " <= ");
  if (type == 0) {
		if (u.assign.e->type == 29) {
			fprintf(stdout, "{");
		}
    PrintExpression(u.assign.e, scope);
  } else if (type == 1) {
		fprintf(stdout, "\\");
    u.recv.chan->Print(stdout);
  } else if (type == 2) {
		if (u.send.se->type == 29) {
			fprintf(stdout, "{");
		}
    PrintExpression(u.send.se, scope);
  }
	fprintf(stdout, " ;\n");
}

void Data::PrintVerilogCondition(int f){
	if (f == 0) {
	  fprintf(stdout, "else if (");
	} else {
	  fprintf(stdout, "if (");
	}
	cond->PrintVerilog(0);
  fprintf(stdout, ")");
}

void Data::PrintVerilogConditionUP(int f){
	if (f == 0) {
  	fprintf(stdout, "else if (");
	} else {
  	fprintf(stdout, "if (");
	}
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
  fprintf(stdout, "[%i:0]\t", dim[0]);

	fprintf(stdout, "\\");
	id->toid()->Print(stdout);
	for (auto i = 1; i < dim.size(); i++) {
    fprintf(stdout, "[%i:0]", dim[i]);
	}
  fprintf(stdout, " ;\n");

	ActId *tmp_id = id->toid();
	if (ischan == 1 && isport == 1) {
		if (type == 0) {
			fprintf(stdout, "reg\t\\");
		} else {
			fprintf(stdout, "wire\t\\");
		}
		tmp_id->Print(stdout);
		fprintf(stdout, "_valid ;\n");
		if (type == 0) {
			fprintf(stdout, "wire\t\\");
		} else {
			fprintf(stdout, "reg\t\\");
		}
		tmp_id->Print(stdout);
		fprintf(stdout, "_ready ;\n");
	} else if (ischan == 1 && isport == 0) {
		fprintf(stdout, "wire\t\\");
		tmp_id->Print(stdout);
		fprintf(stdout, "_valid ;\n");
		fprintf(stdout, "wire\t\\");
		tmp_id->Print(stdout);
		fprintf(stdout, "_ready ;\n");

	}
	delete tmp_id;
}

/*
 *	Port Class
 */
void Port::PrintName(int func){
	if (func == 0) {
		connection->toid()->Print(stdout);
	} else {
		std::string cid = print_array_ref (connection->toid());
		fprintf(stdout, "%s", cid.c_str());
	}
}

void Port::Print(){
  if (dir == 0) {
    if (ischan != 0) {
      fprintf(stdout, "\t,output ");
			if (reg) {
				fprintf(stdout, "reg\t\\");
			} else {
				fprintf(stdout, "\t\\");
			}
      connection->toid()->Print(stdout);
      fprintf(stdout, "_valid\n");
      fprintf(stdout, "\t,input     \t\\");
      connection->toid()->Print(stdout);
      fprintf(stdout, "_ready\n");
    }
		if (ischan != 2) {
    	fprintf(stdout, "\t,output ");
			if (reg) {
				fprintf(stdout, "reg\t");
			} else {
				fprintf(stdout, "\t");
			}
		}
  } else {
    if (ischan != 0)  {
      fprintf(stdout, "\t,output ");
			if (reg) {
      	fprintf(stdout, "reg\t\\");
			} else {
      	fprintf(stdout, "\t\\");
			}
      connection->toid()->Print(stdout);
      fprintf(stdout, "_ready\n");
      fprintf(stdout, "\t,input     \t\\");
      connection->toid()->Print(stdout);
      fprintf(stdout, "_valid\n");
    }
		if (ischan != 2) {
    	fprintf(stdout, "\t,input     \t");
		}
  }
	if (ischan != 2) {
  	fprintf(stdout, "[%i:0]\t\\", width-1);
  	connection->toid()->Print(stdout);
	}
}

/*
 *	Arbiter Class
 */

void Arbiter::PrintInst(int n) {

	fprintf(stdout, "fair_hi #(\n");
	fprintf(stdout, "\t.WIDTH(%lu)\n", a.size());
	fprintf(stdout, ") arb_%i (\n", n);
	fprintf(stdout, "\t .\\clock (\\clock )\n");
	fprintf(stdout, "\t,.\\reset (\\reset )\n");

	fprintf(stdout, "\t,.req ({");
	for (auto i : a) {
		fprintf(stdout, "\\");
		i->PrintVerilog(0);
		if (i != a[a.size()-1]) {
			fprintf(stdout, " ,");
		}
	}
	fprintf(stdout, " })\n");

	fprintf(stdout, "\t,.grant ({");
	for (auto i : a) {
		fprintf(stdout, "\\");
		i->PrintVerilog(2);
		if (i != a[a.size()-1]) {
			fprintf(stdout, " ,");
		}
	}
	fprintf(stdout, " })\n");

	fprintf(stdout, ");\n\n");
}

void Arbiter::PrintArbiter(){
fprintf(stdout, "`timescale 1ns/1ps\n");
fprintf(stdout, "module fair_hi #(\n");
fprintf(stdout, "\tparameter   WIDTH = 8\n");
fprintf(stdout, ")(\n");
fprintf(stdout, "\t\tinput   clock,\n");
fprintf(stdout, "\t\tinput   reset,\n");
fprintf(stdout, "\t\tinput   [WIDTH-1:   0]  req,\n");
fprintf(stdout, "\t\toutput  [WIDTH-1:   0]  grant\n");
fprintf(stdout, "\t);\n");
fprintf(stdout, "\t\n");
fprintf(stdout, "reg\t[WIDTH-1:0]\tpriority [0:WIDTH-1];\n");
fprintf(stdout, "\n");
fprintf(stdout, "reg\t[WIDTH-1:0]\treq_d;\n");
fprintf(stdout, "wire\t[WIDTH-1:0]\treq_neg;\n");
fprintf(stdout, "\n");
fprintf(stdout, "wire shift_prio;\n");
fprintf(stdout, "\n");
fprintf(stdout, "wire [WIDTH-1:0] match;\n");
fprintf(stdout, "reg [WIDTH-1:0] arb_match;\n");
fprintf(stdout, "\n");
fprintf(stdout, "always @(posedge clock)\n");
fprintf(stdout, "if (reset)\n");
fprintf(stdout, "\treq_d <=  0;\n");
fprintf(stdout, "else\n");
fprintf(stdout, "\treq_d <= req;\n");
fprintf(stdout, "\n");
fprintf(stdout, "assign shift_prio = |(req_neg & priority[0]) & !(|arb_match);\n");
fprintf(stdout, "\t\n");
fprintf(stdout, "genvar j;\n");
fprintf(stdout, "genvar i;\n");
fprintf(stdout, "\n");
fprintf(stdout, "generate\n");
fprintf(stdout, "for (j = WIDTH; j > 0; j = j-1) begin    \n");
fprintf(stdout, "\n");
fprintf(stdout, "always @ (*)\n");
fprintf(stdout, "if (j > 1)\n");
fprintf(stdout, "\tif (match[j-1] & &(!match[j-2:0]))\tarb_match[j-1] <= 1'b1;\n");
fprintf(stdout, "\telse\t\t\t\t\t\t\t\tarb_match[j-1] <= 1'b0;\n");
fprintf(stdout, "else\n");
fprintf(stdout, "\tif (match[j-1])\tarb_match[j-1]	<= 1'b1;\n");
fprintf(stdout, "\telse\t\t\tarb_match[j-1]	<= 1'b0;\n");
fprintf(stdout, "end\n");
fprintf(stdout, "endgenerate\n");
fprintf(stdout, "\n");
fprintf(stdout, "generate\n");
fprintf(stdout, "for (j = 0; j < WIDTH; j = j+1) begin\n");
fprintf(stdout, "\n");
fprintf(stdout, "assign req_neg[j] = !req[j] & req_d[j];\n");
fprintf(stdout, "\n");
fprintf(stdout, "assign grant = req & priority[0];\n");
fprintf(stdout, "\n");
fprintf(stdout, "assign match[j] = |(req & priority[j]);\n");
fprintf(stdout, "\n");
fprintf(stdout, "always @(posedge clock)\n");
fprintf(stdout, "if (reset)\n");
fprintf(stdout, "\tpriority[j] <= {{j{1'b0}},1'b1,{(WIDTH-1-j){1'b0}}};\n");
fprintf(stdout, "else if (shift_prio)\n");
fprintf(stdout, "\tif (priority[j] == 1)\n");
fprintf(stdout, "\t\tpriority[j] <= {1'b1, {(WIDTH-1){1'b0}}};\n");
fprintf(stdout, "\telse\n");
fprintf(stdout, "\t\tpriority[j] <= priority[j] >> 1;\n");
fprintf(stdout, "else if (arb_match[j]) begin\n");
fprintf(stdout, "\tpriority[0] <= priority[j];\n");
fprintf(stdout, "\tpriority[j] <= priority[0];\n");
fprintf(stdout, "end\n");
fprintf(stdout, "\t\n");
fprintf(stdout, "end\n");
fprintf(stdout, "endgenerate\n");
fprintf(stdout, "\t\n");
fprintf(stdout, "endmodule\n");
}

}
