/***********************************************************
 * Created on: Jan 25, 2017
 * a=-2 /\ b=3 /\ a>b ==true
 * push 3 equalities
 ***********************************************************/

#include <opensmt/opensmt2.h>
#include <stdio.h>
#include <opensmt/BitBlaster.h>

int main(int argc, char** argv)
{
    SMTConfig c;
    CUFTheory cuftheory(c);
    THandler thandler(c, cuftheory);
    SimpSMTSolver solver(c, thandler);
    MainSolver mainSolver(thandler, c, &solver);
    BVLogic& logic = cuftheory.getLogic();

    PTRef a = logic.mkBVNumVar("a");
    PTRef const1 = logic.mkBVConst(-2);

    PTRef b = logic.mkBVNumVar("b");
    PTRef const2 = logic.mkBVConst(3);

    PTRef eq1 = logic.mkBVEq(a, const1);

	PTRef eq2 = logic.mkBVEq(b, const2);

	vec<PTRef> args;
	args.push(a);
	args.push(b);

    PTRef GT = logic.mkBVSgt(args);

    PTRef one = logic.mkBVConst(1);
    PTRef eq3 = logic.mkBVEq(GT, one);

    SolverId id = { 5 };
	vec<PtAsgn> asgns;
	vec<DedElem> deds;
	vec<PTRef> foo;

	BitBlaster bbb(id, c, mainSolver, logic, asgns, deds, foo);

	BVRef output1;
	lbool stat;
	stat = bbb.insertEq(eq1, output1);

	BVRef output2;
	stat = bbb.insertEq(eq2, output2);

	BVRef output3;
	stat = bbb.insertEq(eq3, output3);

	std::cout << logic.printTerm(eq1) << "\n";
	std::cout << logic.printTerm(eq2) << "\n";
	std::cout << logic.printTerm(eq3) << "\n";

    sstat r = mainSolver.check();

    if (r == s_True)
        printf("sat\n");
    else if (r == s_False)
        printf("unsat\n");
    else if (r == s_Undef)
        printf("unknown\n");
    else
        printf("error\n");

    return 0;
}
