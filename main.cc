#include <stdio.h>
#include <unistd.h>
#include <act/act.h>
#include <act/proto.h>
#include <act/passes/booleanize.h>
#include <act/passes/netlist.h>

void logo () {

fprintf(stdout, "    +  +  +  +  +  +  +  +        +  +  +  +  +  +  +  +        +  +  +  +  +  +  +  +    \n");
fprintf(stdout, "  +-+--+--+--+--+--+--+--+-+    +-+--+--+--+--+--+--+--+-+    +-+--+--+--+--+--+--+--+-+  \n");
fprintf(stdout, "+-+                        +-++-+                        +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |                        |    |                        |  \n");
fprintf(stdout, "+-+                        +-++-+                        +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |       PRS -> FPGA      |    |                        |  \n");
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
  fprintf(stdout, "=============================================================================================\n\n");
  fprintf(stdout, "Usage: fpga_proto [-vOmghp] [-o <*.file>] [-t <*.tcl>] <*.act> 'process_name'\n");
  fprintf(stdout, "-v - Print verilog (has higher priority than graph printing, so don't use them together);\n");
  fprintf(stdout, "-O - Reduced number of flip-flops;\n");
  fprintf(stdout, "-p - Specify process name;\n");
  fprintf(stdout, "-t - Append tcl file to generated tcl script (not supported yet);\n");
  fprintf(stdout, "-o - Save to the file rather then to the stdout;\n");
  fprintf(stdout, "-g - Print graph (for test purposes;\n");
  fprintf(stdout, "-h - Usage guide;\n\n");
  fprintf(stdout, "=============================================================================================\n");
}

int main (int argc, char **argv) { 

  Act *a = NULL;
  char *proc = NULL;

  Act::Init(&argc, &argv);

  int print_or_not_to_print = 0; //that is the question
  int how_to_print = 0;
  int where_to_print = 0;
  int print_g = 0;
  FILE *fout  = stdout;
  char *conf = NULL;

  int key = 0;

  extern int opterr;
  opterr = 0;

  while ((key = getopt (argc, argv, "p:hvOmc:o:t:g")) != -1) {
    switch (key) {
      case 'v':
        print_or_not_to_print = 1;
        break;
      case 'O':
        how_to_print = 1;
        break;
      case 't':
        break;
      case 'c':
        conf = optarg;
        break;
      case 'o':
        fout  = fopen(optarg, "w");
        break;
      case 'g':
        print_g = 1;
        break;
      case 'h':
        usage();
        return 0;
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

	fpga::graph *fg;

	ActBooleanizePass *BOOL = new ActBooleanizePass (a);
	ActNetlistPass *NETL = new ActNetlistPass (a);

	Assert (BOOL->run (p), "Booleanize pass failed");

  NETL->run(p);

  fpga::fpga_config *fc;
  FILE *conf_file;
  conf_file = fopen(conf, "r");
  if (conf_file == 0) {
    fc = fpga::read_config(conf_file, 0);
  } else {
    fc = fpga::read_config(conf_file, 1);
  }
  fclose(conf_file);

  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tBUILDING VERILOG PROJECT...\n");
	fg = fpga::create_fpga_project(a,p);
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tPLACING EXCLUSION MODULES...\n");
  fpga::add_arb(fg);
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tSATISFYING TIMING CONSTRAINTS...\n");
  fpga::add_timing(fg, how_to_print);
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tHANDLING MULTI DRIVERS...\n");
  fpga::add_md(fg);
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  if (print_or_not_to_print == 1) {
    fprintf(stdout, "==========================================\n");
    fprintf(stdout, "\tPRINTING VERILOG...\n");
    fpga::print_verilog(fg, fout);
    fprintf(stdout, "------------------------------------------\n");
    fprintf(stdout, "\tDONE\n");
    fprintf(stdout, "==========================================\n");
  }
  if (print_g == 1) {
    fpga::print_graph(fg, fout);
  }
  return 0;
}
