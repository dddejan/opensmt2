#include "LASolver.h"
#include "LA.h"


static SolverDescr descr_la_solver("LA Solver", "Solver for Quantifier Free Linear Arithmetics");

// MB: helper functions
namespace{
    bool isBoundSatisfied(Delta const & val, LABound const & bound ) {
        if (bound.getType() == bound_u){
            return val <= bound.getValue();
        }
        else {
            assert(bound.getType() == bound_l);
            return val >= bound.getValue();
        }
    }
}



bool LASolver::isValid(PTRef tr)
{
    return logic.isNumLeq(tr); // MB: LA solver expects only inequalities in LEQ form!
}

void LASolver::isProperLeq(PTRef tr)
{
    assert(logic.isAtom(tr));
    assert(logic.isNumLeq(tr));
    Pterm& leq_t = logic.getPterm(tr);
    PTRef cons = leq_t[0];
    PTRef sum  = leq_t[1];
    assert(logic.isConstant(cons));
    assert(logic.isNumVar(sum) || logic.isNumPlus(sum) || logic.isNumTimes(sum));
    assert(!logic.isNumTimes(sum) || ((logic.isNumVar(logic.getPterm(sum)[0]) && (logic.mkNumNeg(logic.getPterm(sum)[1])) == logic.getTerm_NumOne()) ||
                                      (logic.isNumVar(logic.getPterm(sum)[1]) && (logic.mkNumNeg(logic.getPterm(sum)[0])) == logic.getTerm_NumOne())));
}

LASolver::LASolver(SolverDescr dls, SMTConfig & c, LALogic& l, vec<DedElem>& d)
        : logic(l)
//    , bindedRowsStore(l, lva, bra)
//    , pa(pta)
//    , polyStore(lva, pa, bindedRowsStore, l)
        , TSolver((SolverId)descr_la_solver, (const char*)descr_la_solver, c, d)
        //, delta(Delta::ZERO)
        , bland_threshold(1000)
        , lavarStore(l)
        , boundStore(ba, bla, lavarStore, l)
        , model(lavarStore, boundStore, l)
{
    status = INIT;
}

void LASolver::clearSolver()
{
    status = INIT;
    explanationCoefficients.clear();
    candidates.clear();
    // TODO set information about columns and rows in LAVars
    this->tableau.clear();
    removed_by_GaussianElimination.clear();
    TSolver::clearSolver();

    // MB: Let's keep the LAVar store and allocator
//    lva.clear();
//    lavarStore.clear();

    // also keep the bounds allocator, bounds list allocator
//    ba.clear();
//    this->bla.clear();

    this->model.clear();
    // TODO: clear statistics
//    this->tsolver_stats.clear();
    //delta = Delta::ZERO;
}


bool LASolver::check_simplex(bool complete) {
    // opensmt::StopWatch check_timer(tsolver_stats.simplex_timer);
//    printf(" - check %d\n", debug_check_count++);
    (void)complete;
    // check if we stop reading constraints
    if (status == INIT) {
        initSolver();
    }

    bool bland_rule = false;
    unsigned repeats = 0;
    unsigned pivot_counter = 0;
    unsigned bland_counter = 0;
    // These values are from Yices
    unsigned bthreshold = bland_threshold;
    if (nVars() > 10000)
        bthreshold *= 1000;
    else if (nVars() > 1000)
        bthreshold *= 100;

    // keep doing pivotAndUpdate until the SAT/UNSAT status is confirmed
    while (true) {
        repeats++;
        // clear the explanations vector
        explanation.clear( );
        explanationCoefficients.clear( );

        LVRef x = LVRef_Undef;

        if (!bland_rule && (repeats > tableau.getNumOfCols()))
            bland_rule = true;

        if(bland_rule){
            x = getBasicVarToFixByBland();
            ++bland_counter;
            ++tsolver_stats.num_bland_ops;
        }
        else{
            x = getBasicVarToFixByShortestPoly();
            ++pivot_counter;
            ++tsolver_stats.num_pivot_ops;
        }

        if (x == LVRef_Undef) {
            // If not found, check if problem refinement for integers is required
            //if (config.lra_integer_solver && complete)
            //return checkIntegersAndSplit( );

            // Otherwise - SAT
            refineBounds();

//            cerr << "; USUAL SAT" << endl;
            setStatus(SAT);
            break;
//            return setStatus( SAT );
        }

        LVRef y_found = LVRef_Undef;
        if(bland_rule){
            y_found = findNonBasicForPivotByBland(x);
        }
        else{
            y_found = findNonBasicForPivotByHeuristic(x);
        }
        // if it was not found - UNSAT
        if (y_found == LVRef_Undef) {
            vec<PTRef> tmp;
            getConflictingBounds(x, tmp);
            for (int i = 0; i < tmp.size(); i++) {
                PTRef pure;
                lbool sgn;
                logic.purify(tmp[i], pure, sgn);
                explanation.push(PtAsgn(pure, sgn));
            }
            setStatus(UNSAT);
            break;
        }
            // if it was found - pivot old Basic x with non-basic y and do the model updates
        else {
            pivot(x, y_found);
        }
    }
    getStatus() ? tsolver_stats.sat_calls ++ : tsolver_stats.unsat_calls ++;
//    printf(" - check ended\n");
//    printf(" => %s\n", getStatus() ? "sat" : "unsat");
//    if (getStatus())
//        model.printModelState();
    return getStatus();
}

//
// The model system
//
bool LASolver::isModelOutOfBounds(LVRef v) const
{
    return ( (model.read(v) > model.Ub(v)) || (model.read(v) < model.Lb(v)) );
}

bool LASolver::isModelOutOfUpperBound(LVRef v) const
{
    return ( model.read(v)> model.Ub(v) );
}

bool LASolver::isModelOutOfLowerBound(LVRef v) const
{
    return ( model.read(v) < model.Lb(v) );
}


const Delta LASolver::overBound(LVRef v) const
{
    assert( isModelOutOfBounds(v) );
    if (isModelOutOfUpperBound(v))
    {
        return ( Delta(model.read(v) - model.Ub(v)) );
    }
    else if ( isModelOutOfLowerBound(v) )
    {
        return ( Delta(model.Lb(v) - model.read(v)) );
    }
    assert (false);
    printf("Problem in overBound, LRASolver.C:%d\n", __LINE__);
    exit(1);
}

bool LASolver::isEquality(LVRef v) const
{
    return model.isEquality(v);
}

bool LASolver::isUnbounded(LVRef v) const
{
    bool rval = model.isUnbounded(v);
//    if (rval)
//        printf("Var %s is unbounded\n", lva.printVar(v));
    return rval;
}

void LASolver::setBound(PTRef leq_tr)
{
//    printf("Setting bound for %s\n", logic.printTerm(leq_tr));

    boundStore.addBound(leq_tr);
}

opensmt::Number LASolver::getNum(PTRef r) {
    return logic.getNumConst(r);
}


bool LASolver::hasVar(PTRef expr) {
    expr =  logic.isNegated(expr) ? logic.mkNumNeg(expr) : expr;
    PTId id = logic.getPterm(expr).getId();
    return lavarStore.hasVar(id);
}

LVRef LASolver::getLAVar_single(PTRef expr_in) {

    PTRef expr = logic.isNegated(expr_in) ? logic.mkNumNeg(expr_in) : expr_in;

    PTId id_pos = logic.getPterm(expr).getId();

    if (lavarStore.hasVar(id_pos))
        return lavarStore.getVarByPTId(id_pos);

    LVRef x = lavarStore.getNewVar(expr);
    return x;
}

std::unique_ptr<Polynomial> LASolver::expressionToLVarPoly(PTRef term) {
    std::unique_ptr<Polynomial> poly = std::unique_ptr<Polynomial>(new Polynomial());
    // If term is negated, we need to flip the signs of the poly
    bool negated = logic.isNegated(term);
    for (int i = 0; i < logic.getPterm(term).size(); i++) {
        PTRef v;
        PTRef c;
        logic.splitTermToVarAndConst(logic.getPterm(term)[i], v, c);
        LVRef var = getLAVar_single(v);
        notifyVar(var);

        tableau.nonbasicVar(var);

        Real coeff = getNum(c);

        if (negated) {
            coeff.negate();
        }
        poly->addTerm(var, std::move(coeff));
    }
    return poly;
}

//
// Get a possibly new LAVar for a PTRef term.  We may assume that the term is of one of the following forms,
// where x is a real variables and p_i are products of a real variable and a real constant
//
// (1) x
// (2a) (* x -1)
// (2b) (* -1 x)
// (3) x + p_1 + ... + p_n
// (4a) (* x -1) + p_1 + ... + p_n
// (4b) (* -1 x) + p_1 + ... + p_n
//
LVRef LASolver::exprToLVar(PTRef expr) {
    LVRef x = LVRef_Undef;
    if (lavarStore.hasVar(expr)){
        x = lavarStore.getVarByPTId(logic.getPterm(expr).getId());
        if(isProcessedByTableau(x))
        { return x;}
    }

    if (logic.isNumVar(expr) || logic.isNumTimes(expr)) {
        // Case (1), (2a), and (2b)
        PTRef v;
        PTRef c;

        logic.splitTermToVarAndConst(expr, v, c);
        assert(logic.isNumVar(v) || (logic.isNegated(v) && logic.isNumVar(logic.mkNumNeg(v))));
        x = getLAVar_single(v);
        tableau.newNonbasicVar(x);
        notifyVar(x);
    }
    else {
        // Cases (3), (4a) and (4b)
        x = getLAVar_single(expr);
        tableau.newBasicVar(x, expressionToLVarPoly(expr));
    }
    assert(x != LVRef_Undef);
    return x;
}

//
// Reads the constraint into the solver
//
void LASolver::declareAtom(PTRef leq_tr)
{
    if (!logic.isNumLeq(leq_tr)) { return; }

    if (isInformed(leq_tr)) { return; }

    setInformed(leq_tr);


    if (status != INIT)
    {
        // Treat the PTRef as it is pushed on-the-fly
        //    status = INCREMENT;
        assert( status == SAT );
    }
    // DEBUG check
    isProperLeq(leq_tr);

    const Pterm& t = logic.getPterm(leq_tr);

    while (known_preds.size() <= Idx(t.getId()))
        known_preds.push(false);
    known_preds[Idx(t.getId())] = true;
}

void LASolver::informNewSplit(PTRef tr)
{
    PTRef term = logic.getPterm(tr)[1];
    LVRef v = exprToLVar(term);
    lavarStore.addLeqVar(tr, v);
    boundStore.updateBound(tr);
}

LVRef LASolver::getBasicVarToFixByShortestPoly() const {
    decltype(candidates) new_candidates;
    LVRef current = LVRef_Undef;
    std::size_t current_poly_size = static_cast<std::size_t>(-1);
    for (auto it : candidates) {
        assert(tableau.isBasic(it));
        assert(it != LVRef_Undef);
        if (isModelOutOfBounds(it)) {
            new_candidates.insert(it);
            if (current == LVRef_Undef || current_poly_size > tableau.getPolySize(it)) {
                current = it;
                current_poly_size = tableau.getPolySize(it);

            }
        }
    }
    candidates.swap(new_candidates);
    return current;
}

LVRef LASolver::getBasicVarToFixByBland() const {
    decltype(candidates) new_candidates;
    int curr_var_id_x = lavarStore.numVars();
    LVRef current = LVRef_Undef;
    for (auto it : candidates) {
        assert(it != LVRef_Undef);
        assert(tableau.isBasic(it));
        if (isModelOutOfBounds(it)) {
            new_candidates.insert(it);
            // Select the var with the smallest id
            auto id = getVarId(it);
            assert(it.x == id);
            current = id < curr_var_id_x ? it : current;
            curr_var_id_x = id < curr_var_id_x ? id : curr_var_id_x;
        }
    }
    candidates.swap(new_candidates);
    return current;
}

LVRef LASolver::findNonBasicForPivotByHeuristic(LVRef basicVar) {
    // favor more independent variables: those present in less rows
    assert(tableau.isBasic(basicVar));
    LVRef v_found = LVRef_Undef;
    if (model.read(basicVar) < model.Lb(basicVar)) {

        for (auto const &term : tableau.getRowPoly(basicVar)) {
            auto var = term.var;
            assert(tableau.isNonBasic(var));
            assert(var != basicVar);
            auto const &coeff = term.coeff;
            const bool is_coeff_pos = coeff > 0;

            if ((is_coeff_pos && model.read(var) < model.Ub(var)) ||
                (!is_coeff_pos && model.read(var) > model.Lb(var))) {
                if (v_found == LVRef_Undef) {
                    v_found = var;
                }
                    // heuristic favoring more independent vars
                else if (tableau.getColumn(v_found).size() > tableau.getColumn(var).size()) {
                    v_found = var;
                }
            }
        }
    }
    else if (model.read(basicVar) > model.Ub(basicVar)) {

        for (auto const &term : tableau.getRowPoly(basicVar)) {
            auto var = term.var;
            assert(tableau.isNonBasic(var));
            assert(var != basicVar);
            auto const &coeff = term.coeff;
            const bool is_coeff_pos = coeff > 0;

            if ((!is_coeff_pos && model.read(var) < model.Ub(var)) ||
                (is_coeff_pos && model.read(var) > model.Lb(var))) {
                if (v_found == LVRef_Undef) {
                    v_found = var;
                }
                    // heuristic favoring more independent vars
                else if (tableau.getColumn(v_found).size() > tableau.getColumn(var).size()) {
                    v_found = var;
                }
            }
        }
    }
    else{
        opensmt_error( "Error in bounds comparison" );
    }
    return v_found;
}

LVRef LASolver::findNonBasicForPivotByBland(LVRef basicVar) {
    int max_var_id = lavarStore.numVars();
    LVRef y_found = LVRef_Undef;
    // Model doesn't fit the lower bound
    if (model.read(basicVar) < model.Lb(basicVar)) {
        // For the Bland rule
        int curr_var_id_y = max_var_id;
        // look for nonbasic terms to fix the breaking of the bound
        for (auto term : tableau.getRowPoly(basicVar)) {
            auto y = term.var;
            assert(basicVar != y);
            assert(tableau.isNonBasic(y));
            auto const &coeff = term.coeff;
            const bool coeff_is_pos = (coeff > 0);
            if ((coeff_is_pos && model.read(y) < model.Ub(y)) || (!coeff_is_pos && model.read(y) > model.Lb(y))) {
                // Choose the leftmost nonbasic variable with a negative (reduced) cost
                y_found = getVarId(y) < curr_var_id_y ? y : y_found;
                curr_var_id_y = getVarId(y) < curr_var_id_y ? getVarId(y) : curr_var_id_y;
            }
        }
    }
    else if (model.read(basicVar) > model.Ub(basicVar)) {
        int curr_var_id_y = max_var_id;
        // look for nonbasic terms to fix the unbounding
        for (auto term : tableau.getRowPoly(basicVar)) {
            auto y = term.var;
            assert(basicVar != y);
            assert(tableau.isNonBasic(y));
            auto const &coeff = term.coeff;
            const bool &coeff_is_pos = (coeff > 0);
            if ((!coeff_is_pos && model.read(y) < model.Ub(y)) || (coeff_is_pos && model.read(y) > model.Lb(y))) {
                // Choose the leftmost nonbasic variable with a negative (reduced) cost
                y_found = getVarId(y) < curr_var_id_y ? y : y_found;
                curr_var_id_y = getVarId(y) < curr_var_id_y ? getVarId(y) : curr_var_id_y;
            }
        }
    } else {
        opensmt_error("Error in bounds comparison");
    }
    return y_found;
}


//
// Push the constraint into the solver and increase the level
//
bool LASolver::assertLit( PtAsgn asgn, bool reason )
{
    ( void )reason;

//    printf("Assert %d\n", debug_assert_count++);

    // Special cases of the "inequalitites"
    if (logic.isTrue(asgn.tr) && asgn.sgn == l_True) {
        tsolver_stats.sat_calls ++;
        return true;
    }
    if (logic.isFalse(asgn.tr) && asgn.sgn == l_False) {
        tsolver_stats.sat_calls ++;
        return true;
    }
    if (logic.isTrue(asgn.tr) && asgn.sgn == l_False) {
        tsolver_stats.unsat_calls ++;
        return false;
    }
    if (logic.isFalse(asgn.tr) && asgn.sgn == l_True) {
        tsolver_stats.unsat_calls ++;
        return false;
    }
    // check if we stop reading constraints
    if( status == INIT  )
        initSolver( );

    assert(asgn.sgn != l_Undef);

    bool is_reason = false;

    const Pterm& t = logic.getPterm(asgn.tr);


    LABoundRefPair p = boundStore.getBoundRefPair(asgn.tr);
    LABoundRef bound_ref = asgn.sgn == l_False ? p.neg : p.pos;

//    printf("Model state\n");
//    model.printModelState();
//    printf("Asserting %s (%d)\n", boundStore.printBound(bound_ref), asgn.tr.x);
//    printf(" - equal to %s%s\n", asgn.sgn == l_True ? "" : "not ", logic.pp(asgn.tr));

    LVRef it = lavarStore.getVarByLeqId(t.getId());
    // Constraint to push was not found in local storage. Most likely it was not read properly before
    assert(it != LVRef_Undef);
    assert(!isUnbounded(it));


    // skip if it was deduced by the solver itself with the same polarity
    assert(deduced.size() > t.getVar());
    if (deduced[t.getVar()] != l_Undef && deduced[t.getVar()].polarity == asgn.sgn && deduced[t.getVar()].deducedBy == id) {
        assert(getStatus());
        tsolver_stats.sat_calls ++;
        return getStatus();
    }
    if (deduced[t.getVar()] != l_Undef && deduced[t.getVar()].deducedBy == id) {
        is_reason = true; // This is a conflict!
    }

    if (assertBoundOnVar( it, bound_ref))
    {
        assert(getStatus());
        model.pushBound(bound_ref);
        assert(!is_reason);
        if (!is_reason) {
            setPolarity(asgn.tr, asgn.sgn);
            model.pushDecision(asgn);
            getSimpleDeductions(it, bound_ref);
            tsolver_stats.sat_calls++;
        }

    } else {
        tsolver_stats.unsat_calls++;
    }

    return getStatus();
}

bool LASolver::assertBoundOnVar(LVRef it, LABoundRef itBound_ref)
{
    // No check whether the bounds are consistent for the polynomials.  This is done later with Simplex.

    assert( status == SAT );
    assert( it != LVRef_Undef );
    assert( !isUnbounded(it) );
    const LABound &itBound = ba[itBound_ref];

//  cerr << "; ASSERTING bound on " << *it << endl;

    // Check if simple SAT can be given
    if (model.boundSatisfied(it, itBound_ref))
        return getStatus();

    // Check if simple UNSAT can be given.  The last check checks that this is not actually about asserting equality.
    if (model.boundUnsatisfied(it, itBound_ref))
    {
        explanation.clear();
        explanationCoefficients.clear();

        if (itBound.getType() == bound_u)
        {
            PTRef tr = model.readLBound(it).getPTRef();
            explanation.push(PtAsgn(tr, getPolarity(tr)));
            explanationCoefficients.emplace_back( 1 );
        }
        else if (itBound.getType() == bound_l)
        {
            PTRef tr = model.readUBound(it).getPTRef();
            explanation.push(PtAsgn(tr, getPolarity(tr)));
            explanationCoefficients.emplace_back( 1 );
        }

        assert(itBound.getPTRef() != PTRef_Undef);
        explanation.push(itBound.getPtAsgn());
        explanationCoefficients.emplace_back(1);
        return setStatus( UNSAT );
    }

    // Update the Tableau data if a non-basic variable
    if (tableau.isNonBasic(it)) {
        if(!isBoundSatisfied(model.read(it), itBound)){
            changeValueBy(it, itBound.getValue() - model.read(it));
        }
        else{
//            std::cout << "Bound is satisfied by current assignment, no need to update model!\n\n";
        }
    }
    else // basic variable got a new bound, it becomes a possible candidate
    {
        if(!tableau.isActive(it)){
            throw "Not implemented yet!";
        }
        assert(tableau.isBasic(it));
        newCandidate(it);
    }

//  LAVar *x = it;
//  cerr << "; ASSERTED bound on " << *x << ": " << x->L( ) << " <= " << x->M( ) << " <= " << x->U( ) << endl;

//  cerr  << "; NORMAL " << status <<endl;
    return getStatus();
}

//
// Push the solver one level down
//
void LASolver::pushBacktrackPoint( )
{
    // Check if any updates need to be repeated after backtrack
    model.pushBacktrackPoint();
//    printf(" -> Push backtrack point.  Following is the state of the model after the push\n");
//    model.printModelState();

    // Update the generic deductions state
    TSolver::pushBacktrackPoint();
}

// Pop the solver one level up
// NOTE: this method should not be used, pop multiple points is more efficient with popBacktrackPoints rather than popping one by one
void LASolver::popBacktrackPoint( ) {
    this->popBacktrackPoints(1);
}

// Pop the solver given number of times
//
void LASolver::popBacktrackPoints(unsigned int count) {
    for ( ; count > 0; --count){
        PtAsgn dec = model.popBacktrackPoint();
        if (dec != PtAsgn_Undef) {
            clearPolarity(dec.tr);
        }
        TSolver::popBacktrackPoint();
    }
    assert(checkValueConsistency());
//    newCandidate();
    assert(invariantHolds());
    setStatus(SAT);
}

void LASolver::newCandidate(LVRef candidateVar) {
    assert(tableau.isBasic(candidateVar));
    candidates.insert(candidateVar);
}

void LASolver::pivot( const LVRef bv, const LVRef nv){
    assert(tableau.isBasic(bv));
    assert(tableau.isNonBasic(nv));
    assert(valueConsistent(bv));
//    tableau.print();
    updateValues(bv, nv);
    tableau.pivot(bv, nv);
    // after pivot, bv is not longer a candidate
    candidates.erase(bv);
    // and nv can be a candidate
    newCandidate(nv);
//    tableau.print();
    assert(checkTableauConsistency());
    assert(checkValueConsistency());
}

void LASolver::changeValueBy(LVRef var, const Delta & diff) {
    // update var's value
    model.write(var, model.read(var) + diff);
    // update all (active) rows where var is present
    for ( LVRef row : tableau.getColumn(var)){
        assert(tableau.isBasic(row));
        if (tableau.isActive(row)) {
            model.write(row, model.read(row) + (tableau.getCoeff(row, var) * diff));
            newCandidate(row);
        }
    }
}

void LASolver::updateValues(const LVRef bv, const LVRef nv){
    assert(model.read(bv) < model.Lb(bv) || model.read(bv) > model.Ub(bv));
    auto bvNewVal = (model.read(bv) < model.Lb(bv)) ? model.Lb(bv) : model.Ub(bv);
    const auto & coeff = tableau.getCoeff(bv, nv);
    // nvDiff represents how much we need to change nv, so that bv gets to the right value
    auto nvDiff = (bvNewVal - model.read(bv)) / coeff;
    // update nv's value
    changeValueBy(nv, nvDiff);
}


void LASolver::initSolver()
{
    if (status == INIT)
    {
#ifdef GAUSSIAN_DEBUG
        cout << "Columns:" << '\n';
        for (int i = 0; i < columns.size(); i++)
            cout << columns[i] << '\n';
        cout << "Rows:" << '\n';
        for  (int i = 0; i < rows.size(); i++)
            cout << rows[i] << '\n';
#endif
        auto known_PTRefs = getInformed();
        for(PTRef leq_tr : known_PTRefs) {
            Pterm& leq_t = logic.getPterm(leq_tr);

            // Terms are of form c <= t where
            //  - c is a constant and
            //  - t is either a variable or a sum
            PTRef cons = leq_t[0];
            PTRef term = leq_t[1];

            // Ensure that all variables exists, build the polynomial, and update the occurrences.
            LVRef v = exprToLVar(term);

            lavarStore.addLeqVar(leq_tr, v);

            // Assumes that the LRA variable has been already declared
            setBound(leq_tr);
        }
        boundStore.buildBounds(ptermToLABoundRefs); // Bounds are needed for gaussian elimination
        // Gaussian Elimination should not be performed in the Incremental mode!
        if (config.lra_gaussian_elim == 1 && config.do_substitutions())
            doGaussianElimination();

        model.initModel(lavarStore);

        status = SAT;
    }
    else
    opensmt_error( "Solver can not be initialized in the state different from INIT" );
}

//
// Returns boolean value correspondent to the current solver status
//
inline bool LASolver::getStatus( )
{
    switch( status ) {
        case SAT:
        {
            return true;
            break;
        }
        case UNSAT:
        {
            return false;
            break;
        }
        case NEWSPLIT:
        {
            return true;
            break;
        }
        case UNKNOWN:
//            cerr << "LA Solver status is unknown" << endl;
            status = SAT;
            return true;
        case INIT:
        case ERROR:
        default:
        opensmt_error( "Status is undef!" );
            return false;
    }
}

//
// Sets the new solver status and returns the correspondent lbool value
//
bool LASolver::setStatus( LASolverStatus s )
{
    status = s;
    if (s == UNSAT)
        has_explanation = true;
    return getStatus( );
}
//
// Returns the bounds conflicting with the actual model.
//
void LASolver::getConflictingBounds( LVRef x, vec<PTRef> & dst )
{
    if (model.read(x) < model.Lb(x)) {
        // add all bounds for polynomial elements, which limit lower bound
        const LABound& b_f = model.readLBound(x);
        dst.push(b_f.getSign() == l_True ? b_f.getPTRef() : logic.mkNot(b_f.getPTRef()));
//        dst.push(model.readLBound(x).getPTRef());
//        dst.push(ba[bla[lva[x].getBounds()][lva[x].lbound()]].getPTRef());
        explanationCoefficients.emplace_back( 1 );
        for (auto const & term : tableau.getRowPoly(x)) {
            Real const & coeff = term.coeff;
            assert( ! coeff.isZero());
            auto const var = term.var;
            assert(var != x);
            if (coeff < 0) {
                const LABound& b = model.readLBound(var);
                assert( b.getPTRef() != PTRef_Undef );
                dst.push( b.getSign() == l_True ? b.getPTRef() : logic.mkNot(b.getPTRef()) );

                explanationCoefficients.push_back( -coeff );
            }
            else {
                const LABound& b = model.readUBound(var);
                assert( b.getPTRef() != PTRef_Undef );
                dst.push( b.getSign() == l_True ? b.getPTRef() : logic.mkNot(b.getPTRef()) );

                explanationCoefficients.push_back(coeff);
            }
        }

//        for (int i = 0; i < polyStore.getSize(lva[x].getPolyRef()); i++) {
//            const PolyTerm &pt = pta[polyStore.readTerm(polyStore.getPolyRef(x), i)];
//            const Real &a = pt.coef;
//            assert( a != 0 );
//            LVRef y = pt.var;
//            //LVRef y = columns[lva[pt.var].getColId()];
//            assert(x != y);
//
//            if (a < 0) {
//                const LABound& b = model.readLBound(y);
////                printf("The bound is %s\n", b.getSign() == l_True ? "positive" : "negative");
//                assert( b.getPTRef() != PTRef_Undef );
//                dst.push( b.getSign() == l_True ? b.getPTRef() : logic.mkNot(b.getPTRef()) );
//
//                explanationCoefficients.push_back( -a );
//            }
//            else {
//                const LABound& b = model.readUBound(y);
////                printf("The bound is %s\n", b.getSign() == l_True ? "positive" : "negative");
//                assert( b.getPTRef() != PTRef_Undef );
//                dst.push( b.getSign() == l_True ? b.getPTRef() : logic.mkNot(b.getPTRef()) );
//
//                explanationCoefficients.push_back(a);
//            }
//        }
    }
    if (model.read(x) > model.Ub(x)) {
        // add all bounds for polynomial elements, which limit upper bound
//        dst.push(ba[bla[lva[x].getBounds()][lva[x].ubound()]].getPTRef());
        const LABound& b_f = model.readUBound(x);
        dst.push(b_f.getSign() == l_True ? b_f.getPTRef() : logic.mkNot(b_f.getPTRef()));
//        dst.push(model.readUBound(x).getPTRef());
        explanationCoefficients.emplace_back( 1 );

        for (auto const & term : tableau.getRowPoly(x)) {
            Real const & coeff = term.coeff;
            assert( ! coeff.isZero());
            auto const var = term.var;
            assert(var != x);
            if (coeff > 0) {
                const LABound& b = model.readLBound(var);
                assert( b.getPTRef() != PTRef_Undef );
                dst.push( b.getSign() == l_True ? b.getPTRef() : logic.mkNot(b.getPTRef()) );

                explanationCoefficients.push_back( coeff );
            }
            else {
                const LABound& b = model.readUBound(var);
                assert( b.getPTRef() != PTRef_Undef );
                dst.push( b.getSign() == l_True ? b.getPTRef() : logic.mkNot(b.getPTRef()) );

                explanationCoefficients.push_back(-coeff);
            }
        }
//        for (int i = 0; i < polyStore.getSize(lva[x].getPolyRef()); i++) {
//            const PolyTerm &pt = pta[polyStore.readTerm(polyStore.getPolyRef(x), i)];
//            const Real &a = pt.coef;
//            assert( a != 0 );
//            LVRef y = pt.var;
//            //LVRef y = columns[lva[pt.var].getColId()];
//            assert(x != y);
//
//            if (a > 0) {
////                LABoundRef l_bound = bla[lva[y].getBounds()][lva[y].lbound()];
//                const LABound& b = model.readLBound(y);
////                printf("The bound is %s\n", b.getSign() == l_True ? "positive" : "negative");
//                assert( b.getPTRef() != PTRef_Undef );
//                dst.push( b.getSign() == l_True ? b.getPTRef() : logic.mkNot(b.getPTRef()) );
//                explanationCoefficients.push_back( a );
//            }
//            else {
////                LABoundRef u_bound = bla[lva[y].getBounds()][lva[y].ubound()];
//                const LABound& b = model.readUBound(y);
////                printf("The bound is %s\n", b.getSign() == l_True ? "positive" : "negative");
//
//                assert( b.getPTRef() != PTRef_Undef );
//                dst.push( b.getSign() == l_True ? b.getPTRef() : logic.mkNot(b.getPTRef()) );
//                explanationCoefficients.push_back(-a);
//            }
//        }
    }

//    printf("I now came up with an explanation.  It looks like this:\n");
//    for (int i = 0; i < dst.size(); i++)
//        printf("(%s) ", logic.printTerm(dst[i]));
//    printf("\n");

//    assert( dst.size() == polyStore.getSize(lva[x].getPolyRef())+1 ); // One for each term plus the broken equality

}

void LASolver::getSimpleDeductions(LVRef v, LABoundRef br)
{
//    printf("Deducing from bound %s\n", boundStore.printBound(br));
//    printf("The full bound list for %s:\n%s\n", logic.printTerm(lva[v].getPTRef()), boundStore.printBounds(v));

    LABound& bound = ba[br];
    if (bound.getValue().isInf())
        return;
    if (bound.getType() == bound_l) {
        for (int it = bound.getIdx() - 1; it >= 0; it = it - 1) {
            LABoundRef bound_prop_ref = boundStore.getBoundByIdx(v, it);
            LABound &bound_prop = ba[bound_prop_ref];
            if (bound_prop.getValue().isInf() || bound_prop.getType() != bound_l)
                continue;
            deduce(bound_prop);
        }
    } else if (bound.getType() == bound_u) {
        for (int it = bound.getIdx() + 1; it < boundStore.getBoundListSize(v) - 1; it = it + 1) {
            LABoundRef bound_prop_ref = boundStore.getBoundByIdx(v, it);
            LABound & bound_prop = ba[bound_prop_ref];
            if (bound_prop.getValue().isInf() || bound_prop.getType() != bound_u)
                continue;
            deduce(bound_prop);
        }
    }
}

void LASolver::deduce(const LABound & bound_prop) {
    if (!hasPolarity(bound_prop.getPTRef())) {
        if (deduced[logic.getPterm(bound_prop.getPTRef()).getVar()] == l_Undef) {
            lbool pol = bound_prop.getSign();
            deduced[logic.getPterm(bound_prop.getPTRef()).getVar()] = DedElem(id, pol); // id is the solver id
            th_deductions.push(PtAsgn_reason(bound_prop.getPTRef(), pol, PTRef_Undef));
        }
    }
}

//
// Compute the current bounds for each row and tries to deduce something useful
//
void LASolver::refineBounds( )
{

    // Check if polynomial deduction is enabled
    if (config.lra_poly_deduct_size == 0)
        return;
}

//
// Prints the current state of the solver (terms, bounds, tableau)
//
void LASolver::print( ostream & out )
{
    model.printModelState();
    throw "Not implemented yet!";
    // print current non-basic variables
//    out << "Var:" << endl;
//    for ( unsigned i = 0; i < columns.size(); i++ )
//        out << logic.pp(lva[columns[i]].getPTRef()) << "\t";
//    out << endl;
//
//    // print the terms IDs
//    out << "Tableau:" << endl;
//    for ( unsigned i = 0; i < columns.size(); i++)
//        out << lva[columns[i]].ID() << "\t";
//    out << endl;
//
//    // print the Basic/Nonbasic status of terms
//    for ( unsigned i = 0; i < columns.size(); i++)
//        out << ( lva[columns[i]].isBasic() ? "B" : "N" ) << "\t";
//    out << endl;
//
//    // print the tableau cells (zeros are skipped)
//    // iterate over Tableau rows
//    for ( unsigned i = 0; i < rows.size( ); i++ ) {
//        auto const & row_poly = row_polynomials.at(rows[i]);
//        for (unsigned j = 0; j < columns.size(); j++) {
////            if (polyStore.has(lva[rows[i]].getPolyRef(), columns[j]))
////                out << pta[polyStore.find(lva[rows[i]].getPolyRef(), columns[j])].coef;
//            auto it = row_poly.find(columns[j]);
//            if (it != row_poly.end()){
//               out << it->second;
//            }
//            out << "\t";
//        }
//        out << endl;
//    }
}


void LASolver::getConflict(bool, vec<PtAsgn>& e) {
    for (int i = 0; i < explanation.size(); i++) {
        e.push(explanation[i]);

    }
//    printf(" => explanation: \n");
//    for (int i = 0; i < e.size(); i++) {
//        PtAsgn asgn = e[i];
//        LABoundRefPair p = boundStore.getBoundRefPair(asgn.tr);
//        LABoundRef bound_ref = asgn.sgn == l_False ? p.neg : p.pos;
//        printf("(%s) ", boundStore.printBound(bound_ref));
//    }
//    printf("\n");
//    vec<PTRef> check_me;
//    for (int i = 0; i < e.size(); i++) {
//        check_me.push(e[i].sgn == l_False ? logic.mkNot(e[i].tr) : e[i].tr);
//    }
////    printf("In PTRef this is %s\n", logic.pp(logic.mkAnd(check_me)));
//    assert(logic.implies(logic.mkAnd(check_me), logic.getTerm_false()));
}

// We may assume that the term is of the following forms
// (1) (* x c)
// (2) (* c x)
// (3) c
opensmt::Number LASolver::evaluateTerm(PTRef tr)
{
    Pterm& t = logic.getPterm(tr);
    opensmt::Real val(0);
    // Case (3)
    if (logic.isNumConst(tr))
        return logic.getNumConst(tr);

    // Cases (1) & (2)
    PTRef coef;
    PTRef var;
    logic.splitTermToVarAndConst(tr, var, coef);
    PTId id = logic.getPterm(var).getId();
    val += logic.getNumConst(coef) * *concrete_model[getVarId(lavarStore.getVarByPTId(id))];

    return val;
}

// We may assume that the term is of one of the following forms,
// where x is a real variables and p_i are products of a real variable and a real constant
//
// (1a) (* x -1)
// (1b) (* -1 x)
// (2) x + p_1 + ... + p_n
// (3a) (* x -1) + p_1 + ... + p_n
// (3b) (* -1 x) + p_1 + ... + p_n
//
ValPair LASolver::getValue(PTRef tr) {
    if (!logic.hasSortNum(tr)) return ValPair_Undef;
    PTId id = logic.getPterm(tr).getId();
    opensmt::Real val(0);
    if (!lavarStore.hasVar(id)) {
        // This is a linear expression.
        if (logic.getPterm(tr).size() > 0) {
            Pterm &t = logic.getPterm(tr);
            for (int i = 0; i < t.size(); i++)
                val += evaluateTerm(t[i]);
        }
        else
            val = evaluateTerm(tr);
    }
    else
        val = *concrete_model[getVarId(lavarStore.getVarByPTId(id))];

    return ValPair(tr, val.get_str().c_str());
}


bool LASolver::checkValueConsistency() const{
    bool res = true;
    auto const & rows = tableau.getRows();
    for(unsigned i = 0; i < rows.size(); ++i) {
        if(!rows[i]) {continue;}
        LVRef var {i};
        if(tableau.isActive(var)){
            res &= valueConsistent(var);
        }
    }
    assert(res);
    return res;
}

bool LASolver::valueConsistent(LVRef v) const
{
    const Delta& value = model.read(v);
    Delta sum(0);
    for (auto & term : tableau.getRowPoly(v)){
      sum += term.coeff * model.read(term.var);
    }

    assert(value == sum);
    return value == sum;
}

bool LASolver::invariantHolds() const
{
    bool rval = true;
    for (auto var : tableau.getNonBasicVars()){
        assert(model.hasModel(var));
        if (isModelOutOfBounds(var)) {
            rval = false;
            printf("Non-basic (column) LRA var %s has value %s <= %s <= %s\n",
                   printVar(var), model.Lb(var).printValue(),
                   model.read(var).printValue(), model.Ub(var).printValue());
            assert(false);
        }
    }
    return rval;
}
bool LASolver::checkTableauConsistency() const {
    bool res = tableau.checkConsistency();
    assert(res);
    return res;
}


LASolver::~LASolver( )
{
#ifdef STATISTICS
     tsolver_stats.printStatistics(cerr);
#endif // STATISTICS
}

PtAsgn_reason LASolver::getDeduction()  { if (deductions_next >= th_deductions.size()) return PtAsgn_reason_Undef; else return th_deductions[deductions_next++]; }

LALogic&  LASolver::getLogic()  { return logic; }

unsigned LASolver::nVars() const { return lavarStore.numVars(); }

bool LASolver::isProcessedByTableau(LVRef var) {return tableau.isProcessed(var);}

const LABoundRef LASolver::getBound(LVRef v, int idx) const { return boundStore.getBoundByIdx(v, idx); }

