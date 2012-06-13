#import pyximport; pyximport.install()

#from IPython.Debugger import Tracer; debug_here = Tracer()

try:
    from opencog.atomspace import AtomSpace, types, Atom, TruthValue, get_type_name, confidence_to_count
    import opencog.cogserver
except ImportError:
    from atomspace_remote import AtomSpace, types, Atom, TruthValue, get_type_name, confidence_to_count
from tree import *
from util import pp, OrderedSet, concat_lists, inplace_set_attributes
from pprint import pprint
#try:
#    from opencog.util import log
#except ImportError:
from util import log

from collections import defaultdict, Counter
import random
random.seed(42)

import formulas
import rules

from sys import stdout
# You can use kcachegrind on cachegrind.out.profilestats
#from profilestats import profile

#import line_profiler
#profiler = line_profiler.LineProfiler()

from time import time
import exceptions

t = types

def format_log(*args):
    global _line    
    out = '['+str(_line) + '] ' + ' '.join(map(str, args))
    #if _line == 104:
    #    import pdb; pdb.set_trace()
    _line+=1
    return out
_line = 1

class Chainer:

    # Convert Atoms into FakeAtoms for Pypy/Pickle/Multiprocessing compatibility
    _convert_atoms = False

    def __init__(self, space, planning_mode = False):
        self.deduction_types = ['SubsetLink', 'ImplicationLink', 'InheritanceLink', 'PredictiveImplicationLink']

        self.pd = dict()

        self.results = []

        self.space = space
        self.planning_mode = planning_mode
        self.viz = PLNviz(space)
        self.viz.connect()
        self.setup_rules()

        self.bc_later = OrderedSet()
        
        # simple statistics
        self.num_app_pdns_per_level = Counter()
        self.num_uses_per_rule_per_level = defaultdict(Counter)

        global _line
        _line = 1
        
        #profiler.add_function(self.bc)

    #@profile
    def bc(self, target, nsteps = 2000):
        try:
            #import prof3d; prof3d.profile_me()
            
            #try:
            tvs = self.get_tvs(target)
            print "Existing target truth values:", map(str, tvs)
            #if len(tvs):
            #    raise NotImplementedError('Cannot revise multiple TruthValues, so no point in doing this search')
            
            log.info(format_log('bc', target))
            self.bc_later = OrderedSet([target])
            self.results = []
    
            self.target = target
            # Have a sentinel application at the top
            self.root = T('WIN')
            
            dummy_app = rules.Rule(self.root,[target],name='producing target')
            # is this line necessary?
            self.rules.append(dummy_app)
            self.bc_later = OrderedSet([dummy_app])
    
            # viz - visualize the root
            self.viz.outputTarget(target, None, 0, 'TARGET')
    
            steps_so_far = 0
            start = time()
            while self.bc_later and not self.results and steps_so_far < nsteps:
                self.bc_step()
                steps_so_far += 1
    
                #msg = '%s goals expanded, %s remaining, %s Proof DAG Nodes' % (len(self.bc_before), len(self.bc_later), len(self.pd))
                msg = 'done %s steps, %s goals remaining, %s Proof DAG Nodes' % (steps_so_far, len(self.bc_later), len(self.pd))
                log.info(format_log(msg))
                log.info(format_log('time taken', time() - start))
                #log.info(format_log('PD:'))
                #for pdn in self.pd:
                #    log.info(format_log(len(pdn.args),str(pdn)))
            
            # Always print it at the end, so you can easily check the (combinatorial) efficiency of all tests after a change
            print msg
    
            for res in self.results:
                print 'Inference trail:'
                trail = self.trail(res)
                self.print_tree(trail)
                #print 'Action plan (if applicable):'
                #print self.extract_plan(trail)
    #            for res in self.results:
    #                self.viz_proof_tree(self.trail(res))

            viz_path_graph = Viz_Graph()
            self.dsp_search_path(self.expr2pdn(self.target),{},{},viz_path_graph)
            viz_path_graph.write_dot('path_result.dot')
    
            viz_space_graph = Viz_Graph()
            self.dsp_search_space(self.expr2pdn(self.target),viz_space_graph)
            viz_space_graph.write_dot('space_result.dot')
            
            viz_path_graph = Viz_Graph()
            self.dsp_search_path(self.expr2pdn(self.target),{ },{ }, viz_path_graph,True)
            viz_path_graph.write_dot('valid_path_result.dot')
    
            #return self.results
            ret = []
            for tr in self.results:
                atom = atom_from_tree(tr, self.space)
                # haxx
                atom.tv = self.get_tvs(tr)[0]
                ret.append(atom.h)
            
            pprint(self.num_app_pdns_per_level)
            pprint(self.num_uses_per_rule_per_level)
            
            return ret
        except Exception, e:
            import traceback, pdb
            #pdb.set_trace()
            print traceback.format_exc(10)
            # Start the post-mortem debugger
            #pdb.pm()
            return []

    def dsp_search_space(self, dag, viz_graph):
        '''dag : DAG node '''
        # output current node
        for parent in dag.parents:
            parent_id = str(parent)
            target_id = str(dag)
            viz_graph.add_edge(parent_id,target_id)
        # output children
        for arg in dag.args:
            self.dsp_search_space(arg,viz_graph)


    def dsp_search_path(self,dag, dic_count, dic_dag, viz_graph, dsp_valid = False):
        '''dag : DAG node '''
        # output current node
        
        dag_id = str(dag)
        # the map from arg_id to new_arg_id
        new_dic_dag = { }

        if dsp_valid and dag.path_axiom:
            try:
                viz_graph.add_edge(str(dag.path_axiom),dic_dag[dag_id])
                viz_graph.add_edge(str(dag.path_pre),dic_dag[dag_id])
            except Exception:
                viz_graph.add_edge(str(dag.path_axiom),dag_id)
                viz_graph.add_edge(str(dag.path_pre),dag_id)
            print "****" 
            print dag.path_pre
            print dag.path_axiom
        for arg in dag.args:
            #print dag
            #print "***" + str(dag.path_pre)
            if  not dsp_valid or (type(arg.op) == Tree or arg.op.tv.count > 0):
                #self.viz.outputTreeNode(dag,parent,1)
                arg_id = str(arg)
                new_arg_id = None
                try:
                    dic_count[arg_id] += 1
                    new_arg_id = arg_id + str(dic_count[arg_id])
                    new_dic_dag[arg_id] = new_arg_id
                except Exception:
                    dic_count[arg_id] = 1

                if not new_arg_id:
                   new_arg_id = arg_id 

                try:
                    # dag id has been replaced by parent
                    viz_graph.add_edge(dic_dag[dag_id],new_arg_id)
                except Exception:
                    viz_graph.add_edge(dag_id,new_arg_id)
            # output children
        for arg in dag.args:
            if not dsp_valid or (type(arg.op) == Tree or arg.op.tv.count > 0):
                self.dsp_search_path(arg, dic_count, new_dic_dag, viz_graph, dsp_valid)


    #def fc(self):
    #    axioms = [r.head for r in self.rules if r.tv.count > 0]
    #    self.propogate_results_loop(axioms)
    #    
    #    while self.fc_later:
    #        next_premise = self.get_fittest() # Best-first search
    #        log.info(format_log('-FCQ', next_premise))
    #
    #        apps = self.find_new_rule_applications_by_premise(next_premise)
    #        for app in apps:
    #            got_result = self.check_goals_found_already(app)
    #            if got_result:
    #                self.compute_and_add_tv(app)
    #                
    #                if not self.contains_isomorphic_tree(app.head, self.fc_before) and not self.contains_isomorphic_tree(app.head, self.fc_later):
    #                    self.add_tree_to_index(app.head, self.fc_before)
    #                    self.add_tree_to_index(app.head, self.fc_later)
    #                    log.info(format_log('+FCQ', app.head, app.name))
    #                    stdout.flush()

    def bc_step(self):
        # Choose the best app
        # Check for axioms that match its premises
        # If any variables are filled, make a copy of this app
        # Otherwise, check if all premises have been filled
        # If so, compute the TV and produce the head of this app.
        #
        # Note: It will take multiple BC steps to find all the goals for an app.
        # And when it does happen, often you will find all the goals for a clone of the app,
        # and not the original app.
        
        assert self.bc_later
        #print 'bcq', map(str, self.bc_later)
        #next_target = self.bc_later.pop_first() # Breadth-first search
        #next_target = self.bc_later.pop_last() # Depth-first search
        #next_target = self.get_fittest(self.bc_later) # Best-first search
        #next_target = self.select_stochastic() # A better version of best-first search (still a prototype)

        next_app = self.get_fittest(self.bc_later) # Best-first search
        log.info(format_log('-BCQ', next_app))

        # This step will also call propogate_results and propogate_specialization,
        # so it will check for premises, compute the TV if possible, etc.
        self.find_axioms_for_rule_app(next_app)
        
        # This should probably use the extra things in found_axiom
        self.propogate_result(next_app)
        
        #next_target = standardize_apart(next_target)

        for goal in next_app.goals:
            apps = self.find_rule_applications(goal)
    
            for a in apps:
                a = a.standardize_apart()
                self.add_app_if_good(a)
    
        return None

    def contains_isomorphic_tree(self, tr, idx):        
        #return any(expr.isomorphic(tr) for expr in idx)
        canonical = tr.canonical()
        return canonical in idx

    def add_tree_to_index(self, tr, idx):
        canonical = tr.canonical()
        idx.append(canonical)

    def _app_is_stupid(self, goal):

        # You should probably skip the app entirely if it has any self-implying goals
        def self_implication(goal):
            return any(goal.get_type() == get_type(type_name) and len(goal.args) == 2 and goal.args[0].isomorphic(goal.args[1])
                        for type_name in self.deduction_types)

        # Nested ImplicationLinks
        # skip Implications between InheritanceLinks etc as well.
        # Allow an ImplicationLink that implies a PredictiveImplicationLink though.
        types = map(get_type, self.deduction_types)
        if (goal.get_type() in types and len(goal.args) == 2 and
                (goal.args[0].get_type() in types or
                 goal.args[1].get_type() in types)
                and not (goal.get_type() == t.ImplicationLink
                and goal.args[1].get_type() == t.PredictiveImplicationLink
                )
                ):
            return True

        try:
            self._very_vague_link_templates
        except:
            self._very_vague_link_templates = [standardize_apart(T(type, 1, 2)) for type in self.deduction_types]

        # Should actually block this one if it occurs anywhere, not just at the root of the tree.
        #very_vague = any(goal.isomorphic(standardize_apart(T(type, 1, 2))) for type in self.deduction_types)
        very_vague = any(goal.isomorphic(template) for template in self._very_vague_link_templates)
        return (self_implication(goal) or
                     very_vague)

    def reject_expression(self,expr):
        return self._app_is_stupid(expr) or expr.is_variable() or not self.check_domain_recursive(expr)
        
    def check_domain_recursive(self, expr):
        '''Check that any predicates used in the expression are used correctly. It's
        recursive, so it works on e.g. an AndLink containing some EvaluationLinks.'''
        res = all(self.check_domain(subtree) for subtree in expr.flatten())
        #print format_log('check_domain_recursive',expr,res)
        return res
    
    def check_domain(self, expr):
        '''A basic ad-hoc system for checking the domain (i.e. inputs) of a predicate'''
        if expr.is_variable():
            return True
        pred = new_var()
        argument_list = new_var()
        template = T('EvaluationLink',
                     pred,
                     argument_list)
        s = unify(template, expr, {})
        if s == None:
            return True
        # Policy: don't allow the predicate to be a variable
        if s[pred].is_variable():
            return False
        pred_name = s[pred].op.name
        args_trees = s[argument_list].args
        if pred_name == 'neighbor':
            return all(a.is_variable() or a.get_type() == t.ConceptNode for a in args_trees)
        return True

    def add_app_if_good(self, app):
        '''Takes an app. If it is new, and obeys some rules (which filter out useless apps), then add it to the Proof DAG.
        Return the PDN for this app (which is also in canonical form)'''
        def goal_is_stupid(goal):
            return goal.is_variable()

        if any(map(self._app_is_stupid, app.goals)) or self._app_is_stupid(app.head) or not all(g.is_variable() or self.check_domain_recursive(g) for g in list(app.goals)+[app.head]):
            return

        # If the app is a cycle or already added, don't add it or any of its goals
        (status, app_pdn) = self.add_app_to_pd(app)
        if status == 'NEW':
            # Only visualize it if it is actually new
            # viz
            for (i, input) in enumerate(app.goals):
                self.viz.outputTarget(input.canonical(), app.head.canonical(), i, app)

        self.num_app_pdns_per_level[app_pdn.depth] += 1
        self.num_uses_per_rule_per_level[app_pdn.depth][app.name] += 1

        if status == 'CYCLE' or status == 'EXISTING':
            return
        
        assert app not in self.bc_later
        self.bc_later.append(app)
        
        return app_pdn

    #def add_queries(self, app):
    #    def goal_is_stupid(goal):
    #        return goal.is_variable()
    #
    #    if any(map(self._app_is_stupid, app.goals)) or self._app_is_stupid(app.head) or not all(g.is_variable() or self.check_domain_recursive(g) for g in list(app.goals)+[app.head]):
    #        return
    #
    #    # If the app is a cycle or already added, don't add it or any of its goals
    #    (status, app_pdn) = self.add_app_to_pd(app)
    #    if status == 'NEW':
    #        # Only visualize it if it is actually new
    #        # viz
    #        for (i, input) in enumerate(app.goals):
    #            self.viz.outputTarget(input.canonical(), app.head.canonical(), i, app)
    #
    #    if status == 'CYCLE' or status == 'EXISTING':
    #        return
    #    
    #    # NOTE: For generators, the app_pdn will exist already for some reason
    #    # It's useful to add the head if (and only if) it is actually more specific than anything currently in the BC tree.
    #    # This happens all the time when atoms are found.
    #    for goal in tuple(app.goals):
    #        if     not goal_is_stupid(goal):
    #            if  (not self.contains_isomorphic_tree(goal, self.bc_before) and
    #                 not self.contains_isomorphic_tree(goal, self.bc_later) ):
    #                assert goal not in self.bc_before
    #                assert goal not in self.bc_later
    #                self.add_tree_to_index(goal, self.bc_later)
    #                #added_queries.append(goal)
    #                log.info(format_log('+BCQ', goal, app.name))
    #                #stdout.flush()
    #    return app

    def check_goals_found_already(self, app):
        '''Check whether the given app can produce a result. This will happen if all its premises are
        already proven. Or if it is one of the axioms given to PLN initially. It will only find premises
        that are exactly isomorphic to those in the app (i.e. no more specific or general). The chainer
        itself is responsible for finding specific enough apps.'''
        input_tvs = [self.get_tvs(input) for input in app.goals]
        res = all(tvs != [] for tvs in input_tvs)
        #print '########check_goals_found_already',repr(app),res
        return res
    
    def compute_and_add_tv(self, app):
        # NOTE: assumes this is the real copy of the rule, not just a new one.
        #app.tv = True
        input_tvs = [self.get_tvs(g) for g in app.goals]
        if all(input_tvs):
            input_tvs = [tvs[0] for tvs in input_tvs]
            input_tvs = [(float(tv.mean), float(tv.count)) for tv in input_tvs]
            tv_tuple = app.formula(input_tvs,  None)
            app.tv = TruthValue(tv_tuple[0], tv_tuple[1])
            #atom_from_tree(app.head, self.space).tv = app.tv            
            assert app.tv.count > 0
            
            self.set_tv(app, app.tv)
            
            log.info (format_log(app.name, 'produced:', app.head, app.tv, 'using', zip(app.goals, input_tvs)))
            
            # make the app red, not only the expression
            self.viz.declareResult(str(app.canonical_tuple()))

    def find_rule_applications(self, target):
        '''The main 'meat' of the chainer. Finds all possible rule-applications matching your criteria.
        Chainers can be made by searching for certain apps and doing things with them.'''
        ret = []
        for r in self.rules:
            if not r.goals:
                continue
            assert r.match == None
            
            # So the dummy target 'WIN' can only be produced by the dummy app
            #if target == self.root and r.name != 'producing target':
            #    continue
            
            s = unify(r.head, target, {})
            if s != None:
                #if r.name == '[axiom]':
                #    print '>>>>>',repr(r)
                new_rule = r.subst(s)
                ret.append(new_rule)
        return ret

    def find_axioms_for_rule_app(self, app):
        for (i, goal) in enumerate(app.goals):

            def found_axiom(axiom_app, s):
                if axiom_app != None:
                    (status, axiom_app_pdn) = self.add_app_to_pd(axiom_app)
                    if status == 'CYCLE':
                        return None
                    
                    if axiom_app.tv.count > 0:
                        self.viz.outputTarget(None,axiom_app.head,0,axiom_app)
                        self.viz.declareResult(axiom_app.head)
                    
                    # Neither of these options work with ForAllLinks...
                    #gvs = get_varlist(goal)
                    #goal_vars_mapped = [v for v in s.keys() if v in gvs]
                    #if len(goal_vars_mapped) == 0:
                    if len(s) == 0:
                        assert goal == axiom_app.head
                        self.propogate_result(app)
                        
                        # Record the best confidence score below a PDN. This can change
                        # at any time.
                        conf = axiom_app.tv.confidence
                        if conf > 0.0:
                            axiom_expr_pdn = self.expr2pdn(axiom_app.head)
                            self.propogate_scores_upward(axiom_expr_pdn, conf)
                    else:
                        # add specialized trees above this result (if necessary)
                        self.propogate_specialization(axiom_app.head, i, app)
                    
                    return None

            # For example, if you need Inh Mohammad $x, Inh $x terrorist, $x
            # this will avoid finding $x (which could be any atom in the system!) -
            # it'll find the InheritanceLink first and then the ConceptNode
            if self.reject_expression(goal):
                continue
            for r in self.rules:
                axiom_app = None
                if len(r.goals):
                    continue
                
                r = r.standardize_apart()
                
                if r.match == None:
                    s = unify(r.head, goal, {})
                    if s != None:
                        #if r.name == '[axiom]':
                        #    print '>>>>>',repr(r)
                        axiom_app = r.subst(s)
                        
                        found_axiom(axiom_app, s)
                else:
                    # If the Rule has a special function for producing answers, run it
                    # and check the results are valid. NOTE: this algorithm is messy
                    # and may have mistakes
                    s = unify(r.head, goal, {})
                    if s == None:
                        continue
                    new_r = r.subst(s)
                    candidate_heads_tvs = new_r.match(self.space, goal)
                    for (h, tv) in candidate_heads_tvs:
                        s = unify(h, goal, {})
                        if s != None:
                            # Make a new version of the Rule for this Atom
                            #new_rule = r.subst({Var(123):h})
                            axiom_app = new_r.subst(s)
                            axiom_app.head = h
                            # If the Atom has variables, give them values from the target
                            # new_rule = new_rule.subst(s)
                            log.info(format_log('##find_axioms_for_rule_app: printing new magic tv', axiom_app.head, tv))
                            if not tv is None:
                                self.set_tv(axiom_app,tv)
                            
                            found_axiom(axiom_app, s)

    def propogate_result(self, orig_app):
        # app was already precise enough to look up this axiom exactly.
        # Attempt to apply the app using the new TV for the premise
        log.info(format_log('propogate_result',repr(orig_app)))
        app = orig_app
        got_result = self.check_goals_found_already(app)
        if got_result:
            # Only allow one result. NOTE: this will break the Revision rule
            #if len(self.get_tvs(app.head)):
            #    print 'WARNING: rejecting repeated result', app.head
            #    return
            
            log.info(format_log('propogate_result',repr(app)))
            #if app.head.op == 'TARGET':
            #    target = app.goals[0]
            #    log.info(format_log('Target produced!', target))
            #    self.results.append(target)
            #viz
            self.viz.declareResult(app.head)
            self.compute_and_add_tv(app)

            if app.head == self.root:
            #if app.head == self.target:
                log.info(format_log('Target produced!', app.goals[0], self.get_tvs(app.goals[0])))
                self.results.append(app.goals[0])
                return
            
            # then propogate the result further upward
            result_pdn = self.expr2pdn(app.head.canonical())
            upper_pdns = [app_pdn for app_pdn in result_pdn.parents]
            for app_pdn in upper_pdns:
                self.propogate_result(app_pdn.op)
    
    def propogate_specialization(self, new_premise, i, orig_app):
        '''When variables are filled in (i.e. an app is specialized), you also want to
        specialize other apps higher up in the PD (i.e. ones that use this app as their goal).
        new_premise is the new version of some goal in orig_app. It can have variables filled in.
        i is the index for the goal in orig_app matching new_premise.'''
        orig_app = orig_app.standardize_apart()
        #new_premise = standardize_apart(new_premise)
        s = unify(orig_app.goals[i], new_premise, {})
        
        assert s != None
        if s == {}:
            return

        log.info(format_log('propogate_specialization',new_premise,i,repr(orig_app)))
        
        new_app = orig_app.subst(s)        
        new_app.path_pre = orig_app
        new_app.path_axiom = standardize_apart(new_premise)
        
        # Add the new app to the Proof DAG and the backward chaining queue.
        # Stop if it's a cycle or already exists.
        if not self.add_app_if_good(new_app):
            return
        
        orig_app_pdn = self.app2pdn(orig_app)
        assert len(orig_app_pdn.parents) == 1
        orig_result_pdn = orig_app_pdn.parents[0]
        
        new_result = new_app.head
        
        if self.reject_expression(new_result):
            return
        
        # After you finish specializing, it may be a usable rule app
        # (i.e. all premises have been found exactly).
        # This will check, and do nothing if it's not.
        self.propogate_result(new_app)
        
        # Specialize higher-up rules, iff the head has actually been changed
        if new_app.head != orig_app.head:
            # for every app that uses orig_result/new_result as a premise, force that app to
            # be specialized
            for higher_app_pdn in orig_result_pdn.parents:
                # assumes that there is only one goal in an app that will match; that's probably OK
                j = higher_app_pdn.args.index(orig_result_pdn)
                self.propogate_specialization(new_result, j, higher_app_pdn.op)

    # used by forward chaining
    def find_new_rule_applications_by_premise(self,premise):
        ret = []
        for r in self.rules:
            for g in r.goals:
                s = unify(g, premise, {})
                if s != None:
                    new_rule = r.subst(s)
                    ret.append(new_rule)
        return ret


    def get_tvs(self, expr):
        # Only want to find results for this exact target, not every possible specialized version of it.
        # The chaining mechanism itself will create different apps for different specialized versions.
        # If there are any variables in the target, and a TV is found, that means it has been proven
        # for all values of that variable.
        
        canonical = expr.canonical()
        expr_pdn = self.expr2pdn(canonical)
        app_pdns = expr_pdn.args
        #print 'get_tvs:', [repr(app_pdn.op) for app_pdn in app_pdns if app_pdn.tv.count > 0]
        tvs = [app_pdn.tv for app_pdn in app_pdns if app_pdn.tv.count > 0]
        #if len(tvs) > 1:
        #print 'get_tvs',expr,tvs
        return tvs
    
    def expr2pdn(self, expr):
        pdn = DAG(expr,[])
        try:
            return self.pd[pdn]
        except KeyError:
            #print 'expr2pdn adding %s for the first time' % (pdn,)
            self.pd[pdn] = pdn
            return pdn

    def set_tv(self,app,tv):
        assert isinstance(app, rules.Rule)
        # Find/add the app
        a = self.app2pdn(app)
        assert not a is None
        assert isinstance(a, DAG)
        a.tv = tv

    def add_app_to_pd(self,app):
        '''Add app to the Proof DAG. If it is already in the PD then return the existing PDN for that app.
        If it would cause a cycle, reject it. It will convert to canonical form both the app, as well as all
        the goals and the head.'''
        head_pdn = self.expr2pdn(app.head.canonical())

        def canonical_app_goals(goals):
            return map(Tree.canonical, goals)

        goals_canonical = canonical_app_goals(app.goals)
        # Don't allow loops. Does this need to be a separate test? It should probably
        # check whether the new target is more specific, not just equal?
        goal_pdns = [self.expr2pdn(g) for g in goals_canonical]
        if head_pdn.any_path_up_contains(goal_pdns):
            return ('CYCLE',None)
        
        # Check if this application is in the Proof DAG already.
        # NOTE: You must use the app's goals rather than the the app PDN's arguments,
        # because the app's goals may share variables.
        existing = [apn for apn in head_pdn.args if canonical_app_goals(apn.op.goals) == goals_canonical]
        assert len(existing) < 2
        if len(existing) == 1:
            return ('EXISTING',existing[0])
        else:
            # Otherwise add it to the Proof DAG
            app_pdn = DAG(app,[])
            app_pdn.tv = app.tv
            
            #print 'add_app_to_pd:',repr(app)
            
            for goal_pdn in goal_pdns:
                app_pdn.append(goal_pdn)
            head_pdn.append(app_pdn)
        
            #print 'add_app_to_pd adding %s for the first time' % (app_pdn,)
        
            return ('NEW',app_pdn)

    def app2pdn(self,app):
        (status,app_pdn) = self.add_app_to_pd(app)
        assert status != 'CYCLE'
        return app_pdn
    
    def add_rule(self, rule):
        self.rules.append(rule)
        
        # If there's a TruthValue then you can produce the head without requiring any goals.
        # This check should only be done here because a Rule object can also be an app
        assert len(rule.goals) == 0 or rule.tv.confidence == 0
        
        # This is necessary so get_tvs will work
        # Only relevant to generators or axioms
        if rule.tv.confidence > 0:
            self.app2pdn(rule)

    def setup_rules(self):
        self.rules = []
        for r in rules.rules(self.space, self.deduction_types):
            self.add_rule(r)

    def extract_plan(self, trail):
        # The list of actions in a PredictiveImplicationLink. Sometimes there are none,
        # sometimes one; if there is a SequentialAndLink then it can be more than one.
        def actions(proofnode):            
            target = proofnode.op
            if isinstance(target, rules.Rule):
                return []
            elif target.op == 'EvaluationLink':
                tr = target.args[0]
                if tr.op.name == 'agent_location':
                    def get_position(position_name):
                        return tuple(float(s[Var(i)].op.name) for i in [1,2,3])
                    
                    pos_str = target.args[1].args[0].op.name
                    (x,y, _) = get_position(pos_str)
                    return [T('ExecutionLink', self.space.add('goto_obj'),
                                T('ListLink',
                                    "%{x} %{y}" % locals()
                                    )
                             )
                           ]
                else:
                    return []
            #elif target.op in ['ExecutionLink',  'SequentialAndLink']:
            #    return [target]
            elif rules.actionDone_template().unifies(target):
                return target
            else:
                return []
        # Extract all the targets in best-first order
        proofnodes = trail.flatten()
        proofnodes.reverse()
        actions = concat_lists([actions(pn) for pn in proofnodes])
        return actions

    def trail(self, target):
        def rule_found_result(rule_pdn):
            return rule_pdn.tv.count > 0

        def recurse(rule_pdn):
            #print repr(rule_pdn.op),' PDN args', rule_pdn.args
            exprs = map(filter_expr,rule_pdn.args)
            return DAG(rule_pdn.op, exprs)

        def filter_expr(expr_pdn):
            #print expr_pdn.op, [repr(rpdn.op) for rpdn in expr_pdn.args if rule_found_result(rpdn)]
            #successful_rules = [recurse(rpdn) for rpdn in expr_pdn.args if rule_found_result(rpdn)]
            successful_rules = [recurse(rpdn) for rpdn in expr_pdn.args if rule_found_result(rpdn)]
            return DAG(expr_pdn.op, successful_rules)
        
        root = self.expr2pdn(target)

        return filter_expr(root)
    
    def print_tree(self, tr, level = 1):
        #try:
        #    (tr.depth, tr.best_conf_above)
        #    print ' '*(level-1)*3, tr.op, tr.depth, tr.best_conf_above
        #except AttributeError:
        print ' '*(level-1)*3, tr.op
        
        for child in tr.args:
            self.print_tree(child, level+1)

    def propogate_scores_upward(self, expr_pdn, best_conf_below):
        assert isinstance(expr_pdn, DAG)
        assert isinstance(expr_pdn.op, Tree)
        
        # Record the highest confidence of a single expression below this one.
        # This heuristic isn't used yet. It's not ideal - the idea is to estimate
        # the confidence and/or mean that this expression will have, if you search
        # for all ways of producing it. One better option:
        # At each app, take the average "estimated TV" of its goals. And for each goal,
        # take the summed confidence of its rules (or the best, or something - it depends
        # on how you can combine the confidences using Revision)
        expr_pdn.best_conf_below = max(expr_pdn.best_conf_below, best_conf_below)
        
        #print expr_pdn, expr_pdn.best_conf_below
        
        for parent_app_pdn in expr_pdn.parents:
            assert isinstance(parent_app_pdn.op, rules.Rule)
            
            assert len(parent_app_pdn.parents) == 1
            #assert not parent_app_pdn.any_path_up_contains([expr_pdn])
            parent_expr_pdn = parent_app_pdn.parents[0]
            self.propogate_scores_upward(parent_expr_pdn, best_conf_below)

    def pathfinding_score(self, expr):
        def get_coords(expr):
            name = expr.op.name
            tuple_str = name[3:]
            coords = tuple(map(int, tuple_str[1:-1].split(',')))
            return coords

        def get_distance(expr):
            # the location being explored
            l = get_coords(expr)
            # the goal
            t = (90, 90, 98)#get_coords(self.target)
            xdist, ydist, zdist = abs(l[0] - t[0]), abs(l[1] - t[1]), abs(l[2] - t[2])
            dist = xdist+ydist+zdist
            #dist = (xdist**2.0+ydist**2.0+zdist**2.0)**0.5
            return dist
        
        worst_distance = 10**9
        contains_coords = False
        # The greatest distance refered to in the expression
        for subtree in expr.flatten():
            if subtree.get_type() == t.ConceptNode and subtree.op.name.startswith('at '):
                contains_coords = True
                # Hack to skip messy useless Implications that just have
                # the destination + a variable
                if get_distance(subtree) != 0:
                    worst_distance = min(get_distance(subtree), worst_distance)
        
        if contains_coords:
            expr_pdn = self.expr2pdn(expr)
            # I'm not sure this is accurate
            #distance_already = expr_pdn.depth
            return worst_distance
        else:
            return None

    def bc_score(self, expr):
        tmp = self.pathfinding_score(expr)
        
        if not tmp is None:
            return self.pathfinding_score(expr)
        
        expr_pdn = self.expr2pdn(expr)
        
        depth = expr_pdn.depth
        num_vars = len([vertex for vertex in expr.flatten() if expr.is_variable()])
        
        return -10000.0*depth - 100*num_vars

    def get_fittest(self, queue):
        '''[app] -> app
        Finds the best app based on a heuristic score. Currently the score is only based
        on the head of the app and not which Rule is used.'''
        def get_score(app):
            expr = app.head
            expr_pdn = self.expr2pdn(expr)
            
            try:
                return expr_pdn.bc_score
            except AttributeError:
                expr_pdn.bc_score = self.bc_score(expr)
                return expr_pdn.bc_score
        
        # PDNs with the same score will be explored in the order they were added,
        # ie breadth-first search
        best = max(queue, key = get_score)
        #print best
        queue.remove(best)
        
        return best
    
    # TODO make it work now that the backward chaining queue is apps instead of targets.
    def select_stochastic(self):
        '''Choose a PDN to explore next. Based on the first part of Monte Carlo Tree Search.
        Currently only uses the best confidence found in a subtree - the things used in
        bc_score are now redundant.'''
        next_expr_pdn = self.select_stochastic_helper(self.expr2pdn(self.target))
        return next_expr_pdn.op
        
    def select_stochastic_helper(self, expr_pdn):
        if expr_pdn.is_leaf():
            return expr_pdn
        else:
            sub_expr_pdns = []
            for app_pdn in expr_pdn.args:
                # This will automatically skip rules with no goals
                # (e.g. the rule for just looking up an existing atom)
                for subexpr_pdn in app_pdn.args:
                    sub_expr_pdns.append(subexpr_pdn)
            
            # It has been expanded but either a) all the apps are axioms
            # or b) it was impossible to produce it. The following while loop
            # will continue trying out results until something useful is found.
            # It might be simpler to record/update the "number of descendents" in each PDN
            if len(sub_expr_pdns) == 0:
                return None
            
            def get_score(pdn):
                # The score needs to allow exploring things with no existing results
                return pdn.best_conf_below + 1.0
            
            # The probability of choosing a subtree = the probability of it being the best one.
            # You will usually need more than one subtree (because most rules require more than one goal,
            # and you can also combine results using revision)
            while len(sub_expr_pdns):
                total_score = sum(get_score(s) for s in sub_expr_pdns)
                probs = [get_score(s)/total_score for s in sub_expr_pdns]
                # won't work because of floating point error
                #assert sum(probs) == 1
                
                r = random.random()
                index = 0
                while (r > 0):
                    r -= probs[index]
                    index += 1
                
                # We want the last index it saw before r < 0
                index -=1
    
                next_expr_pdn = sub_expr_pdns[index]
                potential_result = self.select_stochastic_helper(next_expr_pdn)
                if (potential_result is None):
                    del sub_expr_pdns[index]
                else:
                    return potential_result
            
            return None

def do_planning(space, target):
    a = space

    rl = T('ReferenceLink', a.add_node(t.ConceptNode, 'plan_selected_demand_goal'), target)
    atom_from_tree(rl, a)
    
    # hack
    print 'target'
    target_a = atom_from_tree(target, a)
    print target_a
    print target_a.tv
    target_a.tv = TruthValue(0,  0)
    print target_a
    
    chainer = Chainer(a, planning_mode = True)        

    result_atoms = chainer.bc(target, 2000)
    
    print "planning result: ",  result_atoms
    
    if result_atoms:
        res_Handle = result_atoms[0]
        res = tree_from_atom(Atom(res_Handle, a))
        
        trail = chainer.trail(res)
        actions = chainer.extract_plan(trail)
        
        # set plan_success
        ps = T('EvaluationLink', a.add_node(t.PredicateNode, 'plan_success'), T('ListLink'))
        # set plan_action_list
        pal = T('ReferenceLink',
                    a.add_node(t.ConceptNode, 'plan_action_list'),
                    T('ListLink', actions))
        
        ps_a  = atom_from_tree(ps, a)
        pal_a = atom_from_tree(pal, a)
        
        print ps
        print pal
        ps_a.tv = TruthValue(1.0, confidence_to_count(1.0))
        
        print ps_a.tv
    
    return result_atoms

from urllib2 import URLError
def check_connected(method):
    '''A nice decorator for use in visualization classes that stream graphs to Gephi. It catches exceptions raised
    when you aren't running Gephi.'''
    def wrapper(self, *args, **kwargs):
        if not self.connected:
            return

        try:
            method(self, *args, **kwargs)
        except URLError:
            self.connected = False

    return wrapper

from collections import defaultdict
import pygephi
class PLNviz:

    def __init__(self, space):
        self._as = space
        self.node_attributes = {'size':10, 'r':0.0, 'g':0.0, 'b':1.0}
        self.rule_attributes = {'size':10, 'r':0.0, 'g':1.0, 'b':1.0}
        self.root_attributes = {'size':20, 'r':1.0, 'g':1.0, 'b':1.0}
        self.result_attributes = {'r':1.0, 'b':0.0, 'g':0.0}

        self.connected = False
        
        self.parents = defaultdict(set)

    def connect(self):
        try:
            self.g = pygephi.JSONClient('http://localhost:8080/workspace0', autoflush=True)
            self.g.clean()
            self.connected = True
        except URLError:
            self.connected = False

    @check_connected
    def outputTarget(self, target, parent, index, rule=None):
        if str(target) == '$1':
            return
        
        self.parents[target].add(parent)

        #target_id = str(hash(target))
        target_id = str(target)

        if parent == None and target != None:
            self.g.add_node(target_id, label=str(target), **self.root_attributes)

        if parent != None:
            # i.e. if it's a generator, which has no goals and only
            # produces a result, which is called 'parent' here
            if target != None:
                self.g.add_node(target_id, label=str(target), **self.node_attributes)

            #parent_id = str(hash(parent))
            #link_id = str(hash(target_id+parent_id))
            parent_id = str(parent)
            #rule_app_id = 'rule '+repr(rule)+parent_id
            rule_app_id = str(rule.canonical_tuple())
            #rule_app_id = 'rule '+repr(rule)+parent_id
            target_to_rule_id = rule_app_id+target_id
            parent_to_rule_id = rule_app_id+' parent'

            self.g.add_node(rule_app_id, label=str(rule), **self.rule_attributes)

            self.g.add_node(parent_id, label=str(parent), **self.node_attributes)

            # Link parent to rule app
            self.g.add_edge(parent_to_rule_id, rule_app_id, parent_id, directed=True, label='')
            # Link rule app to target
            self.g.add_edge(target_to_rule_id, target_id, rule_app_id, directed=True, label=str(index+1))

    @check_connected
    def declareResult(self, target):
        target_id = str(target)
        self.g.change_node(target_id, **self.result_attributes)

        #self.g.add_node(target_id, label=str(target), **self.result_attributes)

    @check_connected
    # More suited for Fishgram
    def outputTreeNode(self, target, parent, index):
        #target_id = str(hash(target))
        target_id = str(target)

        if parent == None:
            self.g.add_node(target_id, label=str(target), **self.root_attributes)

        if parent != None:
            self.g.add_node(target_id, label=str(target), **self.node_attributes)

            #parent_id = str(hash(parent))
            #link_id = str(hash(target_id+parent_id))
            parent_id = str(parent)
            link_id = str(parent)+str(target)

            self.g.add_node(parent_id, label=str(parent), **self.node_attributes)
            self.g.add_edge(link_id, parent_id, target_id, directed=True, label=str(index))

import networkx as nx
class Viz_Graph(object):
    """ draw the graph """
    def __init__(self):
        self._nx_graph = nx.DiGraph()

    def add_edge(self, source, target):
        """docstring for add_edge"""
        self._nx_graph.add_edge(source,target)

    def write_dot(self, filename):
        """docstring for write_dot"""
        try:
            nx.write_dot(self._nx_graph, filename)
        except Exception, e:
            print e
            raise e
    def clear(self):
        """docstring for clear"""
        self._nx_graph.clear()

class PLNPlanningMindAgent(opencog.cogserver.MindAgent):
    '''This agent should be run every cycle to cooperate with the C++
    PsiActionSelectionAgent. This agent adds plans to the AtomSpace, and the
    C++ agent loads plans from the AtomSpace and executes them. (Executing a plan
    can take many cognitive cycles).'''
    def __init__(self):
        self.cycles = 0

    def run(self,atomspace):
        # Currently it has to find the whole plan in one cycle, but that computation
        # should be split into several cycles.
        self.cycles += 1
        
        # hack. don't wait until the plan failed/succeeded, just run planning every
        # cycle, and the plan will be picked up by the C++ PsiActionSelectionAgent
        # when it needs a new plan
        
        # ReferenceLink ConceptNode:'psi_demand_goal_list' (ListLink stuff)
        # or use a hack
        #target_PredicateNodes = [x for x in atomspace.get_atoms_by_type(t.PredicateNode) if "DemandGoal" in x.name]
        target_PredicateNodes = [x for x in atomspace.get_atoms_by_type(t.PredicateNode) if "EnergyDemandGoal" == x.name]
        goal_atom = target_PredicateNodes[0]

        #target = T('EvaluationLink', goal_atom)
        target =    T('EvaluationLink',
                        atomspace.add(t.PredicateNode, name='increased'),
                        T('ListLink',
                            T('EvaluationLink', goal_atom)
                        )
                    )
    
        #try:
        do_planning(atomspace, target)
        #except Exception, e:
        #    log.info(format_log(e))

if __name__ == '__main__':
    a = AtomSpace()
    log.use_stdout(True)

    atom_from_tree(T('EvaluationLink',a.add_node(t.PredicateNode,'A')), a).tv = TruthValue(1, 1)
    #atom_from_tree(T('EvaluationLink',1), a).tv = TruthValue(1, 1)

    c = Chainer(a)

    #search(T('EvaluationLink',a.add_node(t.PredicateNode,'B')))
    #fc(a)

    #c.bc(T('EvaluationLink',a.add_node(t.PredicateNode,'A')))

#    global rules
#    A = T('EvaluationLink',a.add_node(t.PredicateNode,'A'))
#    B = T('EvaluationLink',a.add_node(t.PredicateNode,'B'))
#    rules.append(Rule(B, 
#                                  [ A ]))

    #c.bc(T('EvaluationLink',a.add_node(t.PredicateNode,'A')))
    c.bc(T('EvaluationLink',-1))
