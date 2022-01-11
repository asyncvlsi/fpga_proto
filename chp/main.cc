#include <stdio.h>
#include <unistd.h>
#include <act/act.h>
#include <act/passes/booleanize.h>
#include <act/passes/finline.h>
#include <act/state_machine.h>

void logo () {

fprintf(stdout, "    +  +  +  +  +  +  +  +        +  +  +  +  +  +  +  +        +  +  +  +  +  +  +  +    \n");
fprintf(stdout, "  +-+--+--+--+--+--+--+--+-+    +-+--+--+--+--+--+--+--+-+    +-+--+--+--+--+--+--+--+-+  \n");
fprintf(stdout, "+-+                        +-++-+                        +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |                        |    |                        |  \n");
fprintf(stdout, "+-+                        +-++-+                        +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |       CHP -> FPGA      |    |                        |  \n");
fprintf(stdout, "+-+                        +-++-+     Yale University    +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |        AVLSI Lab       |    |                        |  \n");
fprintf(stdout, "+-+                        +-++-+     Ruslan Dashkin     +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |      Rajit Manohar     |    |                        |  \n");
fprintf(stdout, "+-+                        +-++-+                        +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |                        |    |                        |  \n");
fprintf(stdout, "+-+                        +-++-+                        +-++-+                        +-+\n");
fprintf(stdout, "  +-+--+--+--+--+--+--+--+-+    +-+--+--+--+--+--+--+--+-+    +-+--+--+--+--+--+--+--+-+  \n");
fprintf(stdout, "    +  +  +  +  +  +  +  +        +  +  +  +  +  +  +  +        +  +  +  +  +  +  +  +    \n");

}

void usage () {
  fprintf(stdout, "=============================================================================================\n");
  logo();
  fprintf(stdout, "=============================================================================================\n");
  fprintf(stdout, "Usage: chp2fpga [-p <process_name>] <*.act>\n");
  fprintf(stdout, "=============================================================================================\n");
  fprintf(stdout, "h - Usage guide\n");
  fprintf(stdout, "a - Add arbiter to the print out (only needed for non-det selection)\n");
  fprintf(stdout, "s - System Verilog state machine style\n");
  fprintf(stdout, "o - Output file\n");
  fprintf(stdout, "O<0,1,2,3> - Ooptimization\n");
  fprintf(stdout, "b - Bypass optimization\n");
  fprintf(stdout, "c - Config file input\n");
  fprintf(stdout, "=============================================================================================\n");
}

int main (int argc, char **argv) { 

  Act *a = NULL;
  char *proc = NULL;

  Act::Init(&argc, &argv);

  FILE *fout  = stdout;
  char *conf = NULL;

  int key = 0;

  extern int opterr;
  opterr = 0;

  int parb = 0;
  int sv = 0;
  int opt = 0;

  while ((key = getopt (argc, argv, "p:asho:c:O:")) != -1) {
    switch (key) {
      case 'o':
        if (optarg == NULL) {
          fout = stdout;
        } else {
          fout  = fopen(optarg, "w");
        }
        break;
      case 'h':
        usage();
        exit(1);
        break;
      case 'a':
        parb = 1;
        break;
      case 's':
        sv = 1;
        break;
      case 'c':
        break;
      case 'p':
        if (proc) {
          FREE(proc);
        }
        if (optarg == NULL) {
          fatal_error ("Missing process name");
        }
        proc = Strdup(optarg);
        break;
      case 'O':
        if (optarg == NULL) {
          fatal_error ("Missing optimization level");
        }
        opt = atoi(optarg);
        break;
      case ':':
        fprintf(stderr, "Need a file here\n");
        break;
      case '?':
        usage();
        fatal_error("Something went wrong\n");
        break;
      default: 
        fatal_error("Hmm...\n");
        break;
    }
  }

  if (optind != argc - 1) {
    fatal_error("Missing act file name\n");
  }

  if (proc == NULL) {
    fatal_error("Missing process name\n");
  }
 
  a = new Act(argv[optind]);
  a->Expand ();

	Process *p = a->findProcess (proc);

	if (!p) {
		fatal_error ("Wrong process name, %s", proc);
	}

	if (!p->isExpanded()){
		fatal_error ("Process '%s' is not expanded.", proc);
	}

	ActCHPFuncInline *INLINE = new ActCHPFuncInline (a);
	Assert (INLINE->run(p), "Function inline pass failed");
	INLINE->run(p);

	ActBooleanizePass *BOOL = new ActBooleanizePass (a);
	Assert (BOOL->run(p), "Booleanize pass failed");

  fpga::CHPProject *cp;
 
  cp = fpga::build_machine(a,p,opt);
  
  if (parb == 1) {
	  fpga::Arbiter *arb = new fpga::Arbiter();
	  arb->PrintArbiter();
  }
  
  cp->PrintVerilog(a, sv, fout);

  return 0;
}
