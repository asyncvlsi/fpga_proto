#include <act/act.h>
#include <act/passes/booleanize.h>
#include <act/state_machine.h>
#include <string>

/*
    Since I have too many print functions I moved all of
    them to the separate file :)
 */


namespace fpga {

FILE *output_file = stdout;
static ActBooleanizePass *BOOL = NULL;

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

	char buf[1025];

	id->sPrint(buf, 1025);

	std::string ret_id;

  int first = 0;

	for (auto i = 0; i < 1024; i++) {
		if (buf[i] == 0x5B) {
      if (first == 0) {
			  ret_id += 0x20;
        first = 1;
      }
			ret_id += buf[i];
      if (buf[i+1] < 0x30 || buf[i+1] > 0x39) {
        ret_id += 0x5C;
      }
    } else if (buf[i] == 0x5D) {
			ret_id += 0x20;
			ret_id += buf[i];
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

  if (top) {
    top->PrintPlain();
    PrintPlainState(top->GetNextState());
  }

}

void StateMachine::PrintVerilogVars() {
  for (auto v : vars) {
		if (v->IsPort() == 0) {
			v->PrintVerilog();
		}
  }
  fprintf(output_file, "\n");
}

void StateMachine::PrintVerilogWires(){
  for (auto c : csm) {
    c->PrintVerilogWires();
  }

  for (auto cc : guard_condition) {
    fprintf(output_file, "wire ");
    cc->PrintVerilog(0);
    fprintf(output_file, ";\n");
		if (cc->GetType() == 4) {
			fprintf(output_file, "wire ");
			cc->PrintVerilog(2);
			fprintf(output_file, ";\n");
		}
  }
  for (auto cc : state_condition) {
    fprintf(output_file, "wire ");
    cc->PrintVerilog(0);
    fprintf(output_file, ";\n");
  }
  for (auto cc : commu_condition) {
    fprintf(output_file, "wire ");
    cc->PrintVerilog(0);
    fprintf(output_file, ";\n");
  }
  for (auto cc : comma_condition) {
    fprintf(output_file, "wire ");
    cc->PrintVerilog(0);
    fprintf(output_file, ";\n");
  }
  fprintf(output_file, "\n");
}

void StateMachine::PrintVerilogHeader(int sv) {
  if (p) {
    fprintf(output_file, "`timescale 1ns/1ps\n\n");
    std::string name = get_module_name(p);
    fprintf(output_file, "module \\%s (\n", name.c_str());
    fprintf(output_file, "\t input\t\\clock\n");
    fprintf(output_file, "\t,input\t\\reset\n");
    for (auto pp : ports) {
      pp->Print();
      fprintf(output_file, "\n");
    }
    fprintf(output_file, ");\n\n");
  }

  PrintVerilogWires();
  PrintVerilogVars();
  if (sv == 1) {
    PrintSystemVerilogParameters(0);
  } else {
    PrintVerilogParameters();
  }
  fprintf(output_file, "\n");

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
	  fprintf(output_file, "sm%i_", p->GetNum());
	} else if (f == 2) {
	  fprintf(output_file, "SM%i_", p->GetNum());
	}
  if (p->par) {
    PrintParent(p->GetPar(), f);
  }
}

void StateMachine::PrintSystemVerilogParameters(int f)
{
  //TODO: Make it nice, man...
  if (f == 0) {
    fprintf(output_file,"typedef enum{\n");
    fprintf(output_file,"\tIDLE_LOOP,IDLE_RECV,IDLE_SEND,IDLE_ASSIGN,IDLE_SELECT,IDLE_COMMA,\n");
    fprintf(output_file,"\tEXIT_LOOP,EXIT_RECV,EXIT_SEND,EXIT_ASSIGN,EXIT_SELECT,EXIT_COMMA,\n");
    fprintf(output_file,"\tSELECT_STATEMENT0, SELECT_STATEMENT1,\n");
    fprintf(output_file,"\tSELECT_STATEMENT2, SELECT_STATEMENT3,\n");
    fprintf(output_file,"\tSELECT_STATEMENT4, SELECT_STATEMENT5,\n");
    fprintf(output_file,"\tLOOP_STATEMENT0, LOOP_STATEMENT1,\n");
    fprintf(output_file,"\tLOOP_STATEMENT2, LOOP_STATEMENT3,\n");
    fprintf(output_file,"\tLOOP_STATEMENT4, LOOP_STATEMENT5\n");
    fprintf(output_file,"}states;\n\n");
  }
  if (top) {
  	fprintf(output_file, "states ");
  	top->PrintVerilogName(1);
  	fprintf(output_file, " = IDLE_");
    top->PrintType();
  	fprintf(output_file, ";\n");
  	for (auto i = 0; i < size; i++) {
  	  fprintf(output_file, "localparam ");
  	  top->PrintVerilogName(2);
      if (i == 0) {
        fprintf(output_file, "_%i = IDLE_", i);
        top->PrintType();
        fprintf(output_file, ";\n");
      } else if (i == size - 1) {
        fprintf(output_file, "_%i = EXIT_", i);
        top->PrintType();
        fprintf(output_file, ";\n");
      } else {
    	  fprintf(output_file, "_%i = ", i);
        top->PrintType();
        fprintf(output_file, "_STATEMENT%i;\n", i-1);
      }
  	}
  	fprintf(output_file, "\n");
  	for (auto c : csm) {
  	  c->PrintSystemVerilogParameters(1);
  	}
  }
}

void StateMachine::PrintVerilogParameters(){
	if (top) {
  	fprintf(output_file, "reg [31:0] ");
  	top->PrintVerilogName(1);
  	fprintf(output_file, " = 0;\n");
  	for (auto i = 0; i < size; i++) {
  	  fprintf(output_file, "localparam ");
  	  top->PrintVerilogName(2);
  	  fprintf(output_file, "_%i = %i;\n", i, i);
  	}
  	fprintf(output_file, "\n");
  	for (auto c : csm) {
  	  c->PrintVerilogParameters();
  	}
	}
}

void StateMachine::PrintVerilog() {
  for (auto c : csm) {
    c->PrintVerilog();
    fprintf(output_file, "\n");
  }

	if (top) {
	//  fprintf(output_file, "/*\n\tState Machine type:");
	// 	top->PrintType();
  // 	fprintf(output_file, "*/\n\n");
	} else {
		fprintf(output_file, "/*\tNO CHP BODY IN THE PROCESS\t*/\n");
	}

  for (auto cc : guard_condition) {
    fprintf(output_file, "assign ");
    cc->PrintVerilog(1);
    fprintf(output_file, ";\n");
  }
  for (auto cc : state_condition) {
    fprintf(output_file, "assign ");
    cc->PrintVerilog(1);
    fprintf(output_file, ";\n");
  }
  for (auto cc : commu_condition) {
    fprintf(output_file, "assign ");
    cc->PrintVerilog(1);
    fprintf(output_file, ";\n");
  }
  for (auto cc : comma_condition) {
    fprintf(output_file, "assign ");
    cc->PrintVerilog(1);
    fprintf(output_file, ";\n");
  }
  fprintf(output_file, "\n");

	for (int ii = 0; ii < arb.size(); ii++) {
		arb[ii]->PrintInst(ii);
	}

	if (top) {
  	fprintf(output_file, "always @(posedge \\clock )\n");
  	fprintf(output_file, "if (\\reset ) begin\n\t");
  	fprintf(output_file, "sm%i_", number);
  	if (par) {
  	  PrintParent(par);
  	}
  	fprintf(output_file, "state <= ");
  	fprintf(output_file, "SM%i_", number);
  	if (par) {
  	  PrintParent(par, 2);
  	}
  	fprintf(output_file, "STATE_0;\n");
    fprintf(output_file, "end\n");
	  top->PrintVerilog();
	  PrintVerilogState(top->GetNextState());
    fprintf(output_file, "else begin\n");
    fprintf(output_file, "\tsm%i_", number);
    if (par) { 
      PrintParent(par);
    }
    fprintf(output_file, "state <= sm%i_", number);
    if (par) { 
      PrintParent(par);
    }
    fprintf(output_file, "state;\n");
    fprintf(output_file, "end\n");
	}

  fprintf(output_file, "\n");

  int has_comm = 0;
  int first = 0;

	int ef = 1;

  for (auto id : data) {
    act_boolean_netlist_t *bnl;
    bnl = BOOL->getBNL(GetProc());
    act_dynamic_var_t *dv;
    dv = BOOL->isDynamicRef(bnl, id.first->toid());
    if (dv) {
      int dim = dv->a->nDims();
      for (auto k = 0; k < dim; k++) {
        fprintf(output_file, "integer k%i;\n",k);
      }
    }
    if (id.second[0]->GetType() == 2) {
      fprintf(output_file, "always @(*)\n");
    } else {
      fprintf(output_file, "always @(posedge \\clock )\n");
    }
   	fprintf(output_file, "if (\\reset ) begin\n");
    if (!dv) {
		  fprintf(output_file, "\t\\");
		  id.first->toid()->Print(output_file);
  	  fprintf(output_file, " <= 0;\n");
    } else {
      std::string ind = "\t";
      int dim = 0;
      dim = dv->a->nDims();
      for (auto k = 0; k < dim; k++) {
        fprintf(output_file, "%sfor (k%i=0;k%i<%i;k%i=k%i+1) begin\n", ind.c_str(),
                                                            k,k,
                                                            dv->a->range_size(k),
                                                            k,k);
        ind += "\t";
      }
      fprintf(output_file, "%s\\", ind.c_str());
      id.first->toid()->rootVx(GetProc()->CurScope())->
                        connection()->toid()->Print(output_file);
      fprintf(output_file, " ");
      for (auto k = 0; k < dim; k++) {
        fprintf(output_file, "[k%i]",k);
      }
      fprintf(output_file, " <= 0;\n");
      for (auto k = 0; k < dim; k++) {
        ind.pop_back();
        fprintf(output_file, "%send\n", ind.c_str());
      }
    }
   	fprintf(output_file, "end\n");
	 	ef = 0;
    for (auto dd : id.second) {
			if (dd->GetType() != 2) {
	      dd->PrintVerilogCondition(ef);
			} else {
				dd->PrintVerilogConditionUP(ef);
			}
      fprintf(output_file, " begin\n");
      dd->PrintVerilogAssignment();
      fprintf(output_file, "end\n");
			ef = 0;
    }
		ef = 1;
    fprintf(output_file, "else begin\n");
    if (!dv) {
		  fprintf(output_file, "\t\\");
		  id.first->toid()->Print(output_file);
  	  fprintf(output_file, " <= \\");
		  id.first->toid()->Print(output_file);
      fprintf(output_file, " ;\n");
    }   
    fprintf(output_file, "end\n");
    fprintf(output_file, "\n");
  }

  for (auto id : hs_data) {
		first = 0;
//		ef = 0;
    ef = 2;
  	for (auto dd : id.second) {
  	  if (dd->GetType() == 1 || dd->GetType() == 2) {
  	    if (first == 0) {
  	      dd->PrintVerilogHS(first);
  	      first = 1;
  	    }
        dd->PrintVerilogConditionUP(ef);
        fprintf(output_file, " | ");
 // 	    dd->PrintVerilogCondition(ef);
 // 	    dd->PrintVerilogHS(first);
 // 	    first = 2;
 // 	    dd->PrintVerilogConditionUP(ef);
 // 	    dd->PrintVerilogHS(first);
 // 	    first = 1;
  	  }
  	}
		fprintf(output_file, " 1'b0 ;\n\n");
	}


	for (auto i : inst) {
		i->PrintVerilog();
	}

  if (!par) {
    fprintf(output_file, "\n\nendmodule\n\n");
  }
 
}
/*
 *	State Machine Instance
 */
void StateMachineInst::PrintVerilog(){
	std::string mn = get_module_name(p);
	if (array) {
		fprintf(output_file, "\\%s \\%s%s (\n", mn.c_str(), name->getName(), array);
	} else {
		fprintf(output_file, "\\%s \\%s (\n", mn.c_str(), name->getName());
	}
	fprintf(output_file, "\t.\\clock (\\clock )\n");
	fprintf(output_file, "\t,.\\reset (\\reset )\n");
	for (auto i = 0; i < ports.size(); i++) {
		if (ports[i]->GetChan() != 2) {
			fprintf(output_file, "\t,.\\");
			sm->GetPorts()[i]->PrintName();
			fprintf(output_file, " (\\");
			ports[i]->PrintName();
			fprintf(output_file, " )\n");
		}
		if (ports[i]->GetChan() != 0) {
			fprintf(output_file, "\t,.\\");
			sm->GetPorts()[i]->PrintName();
			fprintf(output_file, "_ready (\\");
			ports[i]->PrintName();
			fprintf(output_file, "_ready )\n");
			fprintf(output_file, "\t,.\\");
			sm->GetPorts()[i]->PrintName();
			fprintf(output_file, "_valid (\\");
			ports[i]->PrintName();
			fprintf(output_file, "_valid )\n");

		}
	}
	fprintf(output_file, ");\n\n");
}


/*
 *  State Class
 */

void State::PrintParent(StateMachine *p, int f = 0) {
	if (f == 0) {
	  fprintf(output_file, "sm%i_", p->GetNum());
	} else if (f == 1) {
		fprintf(output_file, "SM%i_", p->GetNum());
	}
  if (p->GetPar()) {
    PrintParent(p->GetPar(), f);
  }
}

void State::PrintPlain(int p) {
  if (p) {  printed = 1; }
  if (type == ACT_CHP_SEMI) {
    fprintf(output_file, "SEMI\n");
  } else if (type == ACT_CHP_COMMA) {
    fprintf(output_file, "COMMA\n");
  } else if (type == ACT_CHP_SELECT) {
    fprintf(output_file, "SELECT\n");
  } else if (type == ACT_CHP_SELECT_NONDET) {
    fprintf(output_file, "NONDET\n");
  } else if (type == ACT_CHP_LOOP) {
    fprintf(output_file, "LOOP\n");
  } else if (type == ACT_CHP_SKIP) {
    fprintf(output_file, "SKIP\n");
  } else if (type == ACT_CHP_ASSIGN) {
    fprintf(output_file, "ASSIGN\n");
  } else if (type == ACT_CHP_SEND) {
    fprintf(output_file, "SEND\n");
  } else if (type == ACT_CHP_RECV) {
    fprintf(output_file, "RECV\n");
  } else {
    fprintf(output_file, "Not supported type: %i\n", type);
  }
  for (auto c : ns) {
    if (par) PrintParent(par);
    fprintf(output_file, "state%i->", number);
    if (par) PrintParent(par);
    fprintf(output_file, "state%i \n", c.first->GetNum());
    fprintf(output_file, " (");
    c.second->PrintPlain();
    fprintf(output_file, " )\n");
  }
}

void State::PrintType(){
  if (type == ACT_CHP_SEMI) {
    fprintf(output_file, "SEMI");
  } else if (type == ACT_CHP_COMMA) {
    fprintf(output_file, "COMMA");
  } else if (type == ACT_CHP_SELECT) {
    fprintf(output_file, "SELECT");
  } else if (type == ACT_CHP_SELECT_NONDET) {
    fprintf(output_file, "NONDET");
  } else if (type == ACT_CHP_LOOP) {
    fprintf(output_file, "LOOP");
  } else if (type == ACT_CHP_SKIP) {
    fprintf(output_file, "SKIP");
  } else if (type == ACT_CHP_ASSIGN) {
    fprintf(output_file, "ASSIGN");
  } else if (type == ACT_CHP_SEND) {
    fprintf(output_file, "SEND");
  } else if (type == ACT_CHP_RECV) {
    fprintf(output_file, "RECV");
  } else {
    fprintf(output_file, "Not supported type: %i\n", type);
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
    fprintf(output_file, "state_%i", number);
  } else if (f == 1) {
    fprintf(output_file, "state");
  } else if (f == 2) {
		fprintf(output_file, "STATE");
	}
}

void State::PrintVerilog(int p) {
  if (printed) { return; }
  printed = 1;
  for (auto c : ns) {
    c.first->PrintVerilog();
  }
  for (auto c : ns) {
    fprintf(output_file, "else if (");
    c.second->PrintVerilog(0);
    fprintf(output_file, ") begin\n\t");
    if (par) PrintParent(par);
    fprintf(output_file, "state <= ");
    if (par) PrintParent(par,1);
    fprintf(output_file, "STATE_%i", c.first->GetNum());
    fprintf(output_file, ";\n");
    fprintf(output_file, "end\n");
	}

}

/*
 *  Condition Class
 */

void PrintExpression(Expr *e, StateMachine *scope) {
  if (e->type == E_NOT || e->type == E_COMPLEMENT) {
    fprintf(output_file, " ~(");
  } else if (e->type == E_UMINUS) {
    fprintf(output_file, " -(");
  } else if (e->type == E_CONCAT || e->type == E_COMMA) {
  } else {
    fprintf(output_file, "(");
  }
  switch (e->type) {
    case (E_AND): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " & ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_OR): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " | ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_NOT): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_PLUS): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " + ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_MINUS): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " - ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_MULT): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " * ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_DIV): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " / ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_MOD): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " %% ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_LSL): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " << ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_LSR): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " >> ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_ASR): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " >>> ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_UMINUS): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_INT): {
      fprintf(output_file, "32'd%lu", e->u.v);
      break;
    }
    case (E_VAR): {
      ActId *id;
      id = (ActId *)e->u.e.l;
      act_boolean_netlist_t *bnl;
      bnl = BOOL->getBNL(scope->GetProc());
      act_dynamic_var_t *dv;
      dv = BOOL->isDynamicRef(bnl, id);
  		fprintf(output_file, "\\");
      if (!dv) { 
  		  id->Print(output_file);
      } else {
        std::string ts = print_array_ref(id);
        fprintf(output_file, "%s", ts.c_str());
      }
      break;
    }
    case (E_QUERY): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
			PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
			fprintf(output_file, " ? ");
      if (e->u.e.r->u.e.l->type == E_CONCAT || e->u.e.r->u.e.l->type == E_COMMA){fprintf(output_file,"{");}
			PrintExpression(e->u.e.r->u.e.l, scope);
      if (e->u.e.r->u.e.l->type == E_CONCAT || e->u.e.r->u.e.l->type == E_COMMA){fprintf(output_file," }");}
			fprintf(output_file, " : \n\t\t");
      if (e->u.e.r->u.e.r->type == E_CONCAT || e->u.e.r->u.e.r->type == E_COMMA){fprintf(output_file,"{");}
			PrintExpression(e->u.e.r->u.e.r, scope);
      if (e->u.e.r->u.e.r->type == E_CONCAT || e->u.e.r->u.e.r->type == E_COMMA){fprintf(output_file," }");
      }
      break;
    }
    case (E_LPAR): {
			fprintf(output_file, "LPAR\n");
      break;
    }
    case (E_RPAR): {
			fprintf(output_file, "RPAR\n");
      break;
    }
    case (E_XOR): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " ^ ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_LT): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " < ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_GT): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " > ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_LE): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " <=");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;  
    }
    case (E_GE): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " >= ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_EQ): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " == ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_NE): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      fprintf(output_file, " != ");
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.r, scope);
      if (e->u.e.r->type == E_CONCAT || e->u.e.r->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_TRUE): {
      fprintf(output_file, " 1'b1 ");
      break;  
    }
    case (E_FALSE): {
      fprintf(output_file, " 1'b0 ");
      break;
    }
    case (E_COLON): {
      fprintf(output_file, " : ");
      break;
    }
    case (E_PROBE): {
      ActId *id;
			Scope *act_scope;
			act_scope = scope->GetProc()->CurScope();
			id = (ActId *)e->u.e.l;
			act_connection *cc;
			cc = id->Canonical(act_scope);
			StateMachine *tmp;
			tmp = scope->GetPar();
      if (tmp) {
  			while (tmp->GetPar()) {tmp = tmp->GetPar(); }
      }
			fprintf(output_file, "\\");
			id->Print(output_file);
      if (tmp) {
   	    if (tmp->IsPort(cc) == 1) {
				  fprintf(output_file,"_ready");
			  } else if (tmp->IsPort(cc) == 2){
			  	fprintf(output_file,"_valid");
			  }
      } else {
   	    if (scope->IsPort(cc) == 1) {
				  fprintf(output_file,"_ready");
			  } else if (scope->IsPort(cc) == 2){
			  	fprintf(output_file,"_valid");
			  }
      }
      break;
    }
    case (E_COMMA): {
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.r) {
        fprintf(output_file, " ,");
        PrintExpression(e->u.e.r, scope);
      }
      break;
    }
    case (E_CONCAT): {
      PrintExpression(e->u.e.l, scope);
			if (e->u.e.r) {
	      fprintf(output_file, " ,");
  	    PrintExpression(e->u.e.r, scope);
			}
      break;
    }
    case (E_BITFIELD): {
			unsigned int l;
			unsigned int r;
			l = e->u.e.r->u.e.r->u.v;
			if (e->u.e.r->u.e.l) {
				r = e->u.e.r->u.e.l->u.v;
      } else {
				r = l;
			}
			fprintf(output_file, "\\");
      std::string ts = print_array_ref(((ActId *)e->u.e.l));
      fprintf(output_file, "%s", ts.c_str());
      fprintf(output_file, " [");
			if (l!=r) {
				fprintf(output_file, "%i:", l);
				fprintf(output_file, "%i", r);
			} else {
				fprintf(output_file, "%i", r);
			}
      fprintf(output_file, "]");
      break;
    }
    case (E_COMPLEMENT): {
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file,"{");}
      PrintExpression(e->u.e.l, scope);
      if (e->u.e.l->type == E_CONCAT || e->u.e.l->type== E_COMMA){fprintf(output_file," }");}
      break;
    }
    case (E_REAL): {
      fprintf(output_file, "%ld", e->u.v);
      break;
    }
		case (E_ANDLOOP): {
			fprintf(output_file, "ANDLOOP you mean?\n");
			break;
    }
		case (E_ORLOOP): {
			fprintf(output_file, "ORLOOP you mean?\n");
			break;
    }
		case (E_BUILTIN_INT): {
			PrintExpression(e->u.e.l, scope);
			break;
    }
		case (E_BUILTIN_BOOL): {
			PrintExpression(e->u.e.l, scope);
			break;
    }
    case (E_RAWFREE): {
			fprintf(output_file, "RAWFREE\n");
      break;
    }
    case (E_END): {
			fprintf(output_file, "END\n");
      break;
    }
    case (E_NUMBER): {
			fprintf(output_file, "NUMBER\n");
      break;
    }
    case (E_FUNCTION): {
			fprintf(output_file, "FUNCTION\n");
      break;
    }
    default: {
      fprintf(output_file, "Whaaat?! %i\n", e->type);
      break;
    }
  }
  if (e->type == E_CONCAT || e->type == E_COMMA) {
  } else {
    fprintf(output_file, " )");
  }

}

void Condition::PrintExpr(Expr *e) {
  PrintExpression(e, scope);
}

void Condition::PrintScope(StateMachine *sc, int f = 0){
	if (f == 0) {
	  fprintf(output_file, "sm%i_", sc->GetNum());
	} else if (f == 1) {
	  fprintf(output_file, "SM%i_", sc->GetNum());
	} else if (f == 2) {
	  fprintf(stdout, "sm%i_", sc->GetNum());
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
      PrintScope(scope,2);
      fprintf(stdout, "commu_compl = ");
      u.v->Print(output_file);
      fprintf(stdout, "_valid & ");
      u.v->Print(output_file);
      fprintf(stdout, "_ready");
      break;
    case (1) :
      PrintScope(scope,2);
      fprintf(stdout, "guard = ");
      PrintExpr(u.e);
      break;
    case (2) :
      sm = u.s->GetPar();
      PrintScope(scope,2);
      fprintf(stdout, "state_cond_%i = ", num);
      PrintScope(sm,2);
      fprintf(stdout, "state == "); 
      PrintScope(sm,2);
      fprintf(stdout, "state_%i", u.s->GetNum());
      break;
    case (3) :
      PrintScope(scope,2);
      fprintf(stdout, "cond_%i = ", num);
      fprintf(stdout, "(");
      for (auto cc : u.c->c) {
        if (u.c->type == 2) {
          fprintf(stdout, "!");
        }
        PrintScope(cc->GetScope(),2);
        if (cc->GetType() == 0) {
          fprintf(stdout, "commun_compl_%i ", cc->GetNum());
        } else if (cc->GetType() == 1) {
          fprintf(stdout, "guard_%i ", cc->GetNum());
        } else if (cc->GetType() == 2) {
          State *ss = cc->GetState();
          sm = ss->GetPar();
          fprintf(stdout, "state_cond_%i ", cc->GetNum());
        } else {
          fprintf(stdout, "cond_%i ", cc->GetNum());
        }
        if (cc != u.c->c[u.c->c.size()-1]) {
          if (u.c->type != 1) {
            fprintf(stdout," & ");
          } else {
            fprintf(stdout," | ");
          }
        }
      }
      fprintf(stdout, ")");
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
        fprintf(output_file, "commu_compl_%i", num);
        break;
      case (1) :
        PrintScope(scope);
        fprintf(output_file, "guard_%i", num);
        break;
      case (2) :
        PrintScope(scope);
        fprintf(output_file, "state_cond_%i", num);
        break;
      case (3) :
        PrintScope(scope);
        fprintf(output_file, "cond_%i", num);
        break;
      case (4) :
        PrintScope(scope);
        fprintf(output_file, "excl_guard_%i", num);
        break;
      default :
        fatal_error("!!!\n");
    }
  } else if (f == 1) {
    switch (type) {
      case (0) :
        PrintScope(scope);
        fprintf(output_file, "commu_compl_%i = \\", num);
        u.v->Print(output_file);
        fprintf(output_file, "_valid & \\");
        u.v->Print(output_file);
        fprintf(output_file, "_ready ");
        break;
      case (1) :
        PrintScope(scope);
        fprintf(output_file, "guard_%i = ", num);
        PrintExpr(u.e);
        fprintf(output_file, " ? 1'b1 : 1'b0 ");
        break;
      case (2) :
        sm = u.s->GetPar();
        PrintScope(scope);
        fprintf(output_file, "state_cond_%i = ", num);
        PrintScope(sm);
        fprintf(output_file, "state == "); 
        PrintScope(sm,1);
        fprintf(output_file, "STATE_%i", u.s->GetNum());
        break;
      case (3) :
        PrintScope(scope);
        fprintf(output_file, "cond_%i = ", num);
      	for (auto cc : u.c->c) {	
      	  if (u.c->type == 2) {
      	    fprintf(output_file, "!");
      	  }
      	  PrintScope(cc->GetScope());
      	  if (cc->GetType() == 0) {
      	    fprintf(output_file, "commu_compl_%i", cc->GetNum());
      	  } else if (cc->GetType() == 1) {
      	    fprintf(output_file, "guard_%i", cc->GetNum());
      	  } else if (cc->GetType() == 2) {
      	    State *ss = cc->GetState();
      	    sm = ss->GetPar();
      	    fprintf(output_file, "state_cond_%i", cc->GetNum());
      	  } else {
      	    fprintf(output_file, "cond_%i", cc->GetNum());
      	  }
      	  if (cc != u.c->c[u.c->c.size()-1]) {
      	    if (u.c->type != 1) {
      	      fprintf(output_file," & ");
      	    } else {
      	      fprintf(output_file," | ");
      	    }
					}
      	}
        break;
      case (4) :
        PrintScope(scope);
        fprintf(output_file, "excl_guard_%i = ", num);
        PrintExpr(u.e);
        fprintf(output_file, " ? 1'b1 : 1'b0 ");
        break;
      default :
        fatal_error("!!!\n");
    }
  } else if (f == 2) {
		PrintScope(scope);
		fprintf(output_file, "guard_%i", num);
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
  
  fprintf(output_file, "always @(*)\n\t\\");
  if (type == 1) {
    u.recv.chan->Print(output_file);
  } else if (type == 2) {
    id->Print(output_file);
  }
  if (scope->IsPort(cid) == 1) {
    fprintf(output_file, "_valid <= ");
  } else if (scope->IsPort(cid) == 2) {
    fprintf(output_file, "_ready <= ");
  } else {
    fprintf(output_file, "Whaaaat?!\n");
  }
}
/*
  if (f == 0) {
    fprintf(output_file, "always @(posedge \\clock )\n");
    fprintf(output_file, "if (\\reset ) begin\n\t\\");
    if (type == 1) {
      u.recv.chan->Print(output_file);
    } else if (type == 2) {
      id->Print(output_file);
    }
    if (scope->IsPort(cid) == 1) {
      fprintf(output_file, "_valid <= 1'b0;\n");
    } else if (scope->IsPort(cid) == 2) {
      fprintf(output_file, "_ready <= 1'b0;\n");
    } else {
      fprintf(output_file, "Whaaaat?!\n");
    }
    fprintf(output_file, "end\n");
  } else {
    fprintf(output_file, " begin\n\t\\");
    if (type == 1) {
      u.recv.chan->Print(output_file);
    } else if (type == 2) {
      id->Print(output_file);
    }
    if (f == 1) {
      if (scope->IsPort(cid) == 1) {
        fprintf(output_file, "_valid <= 1'b0;\n");
      } else if (scope->IsPort(cid) == 2) {
        fprintf(output_file, "_ready <= 1'b0;\n");
      } else {
        fprintf(output_file, "Whaaaat?!\n");
      }
      fprintf(output_file, "end\n");
    } else if (f == 2) {
      if (scope->IsPort(cid) == 1) {
        fprintf(output_file, "_valid <= 1'b1;\n");
      } else if (scope->IsPort(cid) == 2) {
        fprintf(output_file, "_ready <= 1'b1;\n");
      } else {
        fprintf(output_file, "Whaaaat?!\n");
      }
      fprintf(output_file, "end\n");
    }
  }
}
*/

void Data::PrintVerilogAssignment() {
  if (printed) {return;}
  printed = 1;
	fprintf(output_file, "\t\\");
  act_boolean_netlist_t *bnl;
  bnl = BOOL->getBNL(scope->GetProc());
  act_dynamic_var_t *dv;
  dv = BOOL->isDynamicRef(bnl, id);
  if (!dv) { 
    id->Print(output_file);
  } else {
    std::string ts = print_array_ref(id);
    fprintf(output_file, "%s", ts.c_str());
  }
	fprintf(output_file, " <= ");
  if (type == 0) {
		if (u.assign.e->type == E_CONCAT | 
        u.assign.e->type == E_COMMA) {
			fprintf(output_file, "{");
		}
    PrintExpression(u.assign.e, scope);
    if (u.assign.e->type == E_CONCAT |
        u.assign.e->type == E_COMMA) {
      fprintf(output_file, " }");
    }
  } else if (type == 1) {
		fprintf(output_file, "\\");
    u.recv.chan->Print(output_file);
  } else if (type == 2) {
		if (u.send.se->type == E_CONCAT |
        u.send.se->type == E_COMMA) {
			fprintf(output_file, "{");
		}
    PrintExpression(u.send.se, scope);
    if (u.send.se->type == E_CONCAT |
        u.send.se->type == E_COMMA) {
      fprintf(output_file, " }");
    } 
  }
	fprintf(output_file, " ;\n");
}

void Data::PrintVerilogCondition(int f){
	if (f == 0) {
	  fprintf(output_file, "else if (");
	} else if (f == 1) {
	  fprintf(output_file, "if (");
	}
	cond->PrintVerilog(0);
  if (f == 0 | f == 1) {
    fprintf(output_file, ")");
  }
}

void Data::PrintVerilogConditionUP(int f){
	if (f == 0) {
  	fprintf(output_file, "else if (");
	} else if (f == 1) {
  	fprintf(output_file, "if (");
	}
  if (type == 1) {
    u.recv.up_cond->PrintVerilog(0);
  } else if (type == 2) {
    u.send.up_cond->PrintVerilog(0);
  }
  if (f == 0 | f == 1) {
    fprintf(output_file, ")");
  }
}

/*
 *  Variable class
 */
void Variable::PrintVerilog (){
  if (type == 0) {
    fprintf(output_file, "reg\t");
  } else {
    fprintf(output_file, "wire\t");
  }
  if (dim[0] >= 1) {
    fprintf(output_file, "[%i:0]\t", dim[0]);
  } else {
    fprintf(output_file, "\t");
  }
  fprintf(output_file, "\\");
	id->toid()->Print(output_file);
	for (auto i = 1; i < dim.size(); i++) {
    if (isdyn == 1) { fprintf(output_file, " "); }
    fprintf(output_file, "[%i:0]", dim[i]-1);
	}
  fprintf(output_file, " ;\n");
  
	ActId *tmp_id = id->toid();
	if (ischan == 1 && isport == 1) {
		if (type == 0) {
			fprintf(output_file, "reg\t\\");
		} else {
			fprintf(output_file, "wire\t\\");
		}
		tmp_id->Print(output_file);
		fprintf(output_file, "_valid ;\n");
		if (type == 0) {
			fprintf(output_file, "wire\t\\");
		} else {
			fprintf(output_file, "reg\t\\");
		}
		tmp_id->Print(output_file);
		fprintf(output_file, "_ready ;\n");
	} else if (ischan == 1 && isport == 0) {
		fprintf(output_file, "wire\t\\");
		tmp_id->Print(output_file);
		fprintf(output_file, "_valid ;\n");
		fprintf(output_file, "wire\t\\");
		tmp_id->Print(output_file);
		fprintf(output_file, "_ready ;\n");
  
	}
	delete tmp_id;
}

/*
 *	Port Class
 */
void Port::PrintName(int func){
	if (func == 0) {
		connection->toid()->Print(output_file);
	} else {
		std::string cid = print_array_ref (connection->toid());
		fprintf(output_file, "%s", cid.c_str());
	}
}

void Port::Print(){
  if (dir == 0) {
    if (ischan != 0) {
      fprintf(output_file, "\t,output ");
			if (reg) {
				fprintf(output_file, "reg\t\\");
			} else {
				fprintf(output_file, "\t\\");
			}
      connection->toid()->Print(output_file);
      fprintf(output_file, "_valid\n");
      fprintf(output_file, "\t,input     \t\\");
      connection->toid()->Print(output_file);
      fprintf(output_file, "_ready\n");
    }
		if (ischan != 2) {
    	fprintf(output_file, "\t,output ");
			if (reg) {
				fprintf(output_file, "reg\t");
			} else {
				fprintf(output_file, "\t");
			}
		}
  } else {
    if (ischan != 0)  {
      fprintf(output_file, "\t,output ");
			if (reg) {
      	fprintf(output_file, "reg\t\\");
			} else {
      	fprintf(output_file, "\t\\");
			}
      connection->toid()->Print(output_file);
      fprintf(output_file, "_ready\n");
      fprintf(output_file, "\t,input     \t\\");
      connection->toid()->Print(output_file);
      fprintf(output_file, "_valid\n");
    }
		if (ischan != 2) {
    	fprintf(output_file, "\t,input     \t");
		}
  }
	if (ischan != 2) {
  	fprintf(output_file, "[%i:0]\t\\", width-1);
  	connection->toid()->Print(output_file);
	}
}

/*
 *	Arbiter Class
 */

void Arbiter::PrintInst(int n) {

	fprintf(output_file, "fair_hi #(\n");
	fprintf(output_file, "\t.WIDTH(%lu)\n", a.size());
	fprintf(output_file, ") arb_%i (\n", n);
	fprintf(output_file, "\t .\\clock (\\clock )\n");
	fprintf(output_file, "\t,.\\reset (\\reset )\n");

	fprintf(output_file, "\t,.req ({");
	for (auto i : a) {
		fprintf(output_file, "\\");
		i->PrintVerilog(0);
		if (i != a[a.size()-1]) {
			fprintf(output_file, " ,");
		}
	}
	fprintf(output_file, " })\n");

	fprintf(output_file, "\t,.grant ({");
	for (auto i : a) {
		fprintf(output_file, "\\");
		i->PrintVerilog(2);
		if (i != a[a.size()-1]) {
			fprintf(output_file, " ,");
		}
	}
	fprintf(output_file, " })\n");

	fprintf(output_file, ");\n\n");
}

void Arbiter::PrintArbiter(){
fprintf(output_file, "`timescale 1ns/1ps\n");
fprintf(output_file, "module fair_hi #(\n");
fprintf(output_file, "\tparameter   WIDTH = 8\n");
fprintf(output_file, ")(\n");
fprintf(output_file, "\t\tinput   clock,\n");
fprintf(output_file, "\t\tinput   reset,\n");
fprintf(output_file, "\t\tinput   [WIDTH-1:   0]  req,\n");
fprintf(output_file, "\t\toutput  [WIDTH-1:   0]  grant\n");
fprintf(output_file, "\t);\n");
fprintf(output_file, "\t\n");
fprintf(output_file, "reg\t[WIDTH-1:0]\tpriorit [0:WIDTH-1];\n");
fprintf(output_file, "\n");
fprintf(output_file, "reg\t[WIDTH-1:0]\treq_d;\n");
fprintf(output_file, "wire\t[WIDTH-1:0]\treq_neg;\n");
fprintf(output_file, "\n");
fprintf(output_file, "wire shift_prio;\n");
fprintf(output_file, "\n");
fprintf(output_file, "wire [WIDTH-1:0] match;\n");
fprintf(output_file, "reg [WIDTH-1:0] arb_match;\n");
fprintf(output_file, "\n");
fprintf(output_file, "always @(posedge clock)\n");
fprintf(output_file, "if (reset)\n");
fprintf(output_file, "\treq_d <=  0;\n");
fprintf(output_file, "else\n");
fprintf(output_file, "\treq_d <= req;\n");
fprintf(output_file, "\n");
fprintf(output_file, "assign shift_prio = |(req_neg & priorit[0]) & !(|arb_match);\n");
fprintf(output_file, "\t\n");
fprintf(output_file, "genvar j;\n");
fprintf(output_file, "genvar i;\n");
fprintf(output_file, "\n");
fprintf(output_file, "generate\n");
fprintf(output_file, "for (j = WIDTH; j > 0; j = j-1) begin    \n");
fprintf(output_file, "\n");
fprintf(output_file, "always @ (*)\n");
fprintf(output_file, "if (j > 1)\n");
fprintf(output_file, "\tif (match[j-1] & &(!match[j-2:0]))\tarb_match[j-1] <= 1'b1;\n");
fprintf(output_file, "\telse\t\t\t\t\t\t\t\tarb_match[j-1] <= 1'b0;\n");
fprintf(output_file, "else\n");
fprintf(output_file, "\tif (match[j-1])\tarb_match[j-1]	<= 1'b1;\n");
fprintf(output_file, "\telse\t\t\tarb_match[j-1]	<= 1'b0;\n");
fprintf(output_file, "end\n");
fprintf(output_file, "endgenerate\n");
fprintf(output_file, "\n");
fprintf(output_file, "generate\n");
fprintf(output_file, "for (j = 0; j < WIDTH; j = j+1) begin\n");
fprintf(output_file, "\n");
fprintf(output_file, "assign req_neg[j] = !req[j] & req_d[j];\n");
fprintf(output_file, "\n");
fprintf(output_file, "assign grant = req & priorit[0];\n");
fprintf(output_file, "\n");
fprintf(output_file, "assign match[j] = |(req & priorit[j]);\n");
fprintf(output_file, "\n");
fprintf(output_file, "always @(posedge clock)\n");
fprintf(output_file, "if (reset)\n");
fprintf(output_file, "\tpriorit[j] <= {{j{1'b0}},1'b1,{(WIDTH-1-j){1'b0}}};\n");
fprintf(output_file, "else if (shift_prio)\n");
fprintf(output_file, "\tif (priorit[j] == 1)\n");
fprintf(output_file, "\t\tpriorit[j] <= {1'b1, {(WIDTH-1){1'b0}}};\n");
fprintf(output_file, "\telse\n");
fprintf(output_file, "\t\tpriorit[j] <= priorit[j] >> 1;\n");
fprintf(output_file, "else if (arb_match[j]) begin\n");
fprintf(output_file, "\tpriorit[0] <= priorit[j];\n");
fprintf(output_file, "\tpriorit[j] <= priorit[0];\n");
fprintf(output_file, "end\n");
fprintf(output_file, "\t\n");
fprintf(output_file, "end\n");
fprintf(output_file, "endgenerate\n");
fprintf(output_file, "\t\n");
fprintf(output_file, "endmodule\n");
}

}
