/*
 * Copyright (C) 2002-2007 Novamente LLC
 * Copyright (C) 2008 by Singularity Institute for Artificial Intelligence
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef SIMPLEANDRULE_H
#define SIMPLEANDRULE_H

#include <boost/lexical_cast.hpp>

namespace opencog { namespace pln {

const bool RuleResultFreshness = true;
pHandle UnorderedCcompute(AtomSpaceWrapper *asw,
                          Type linkT, const ArityFreeFormula<TruthValue,
                                                             TruthValue*>& fN,
                          pHandle* premiseArray,
                          const int n, pHandle CX=PHANDLE_UNDEFINED);

template<int N>
class SimpleANDRule : public ArityFreeANDRule
{
public:
	SimpleANDRule(AtomSpaceWrapper *_asw)
	: ArityFreeANDRule(_asw)
	{
		name = "SimpleANDRule" + boost::lexical_cast<std::string>(N);
		for (int i = 0; i < N; i++)
			inputFilter.push_back(meta(
				new tree<Vertex>(mva((pHandle)ATOM))
			));
	}
	bool validate2(MPs& args) const { return true; }

	Rule::setOfMPs o2iMetaExtra(meta outh, bool& overrideInputFilter) const
	{
		if (!asw->inheritsType(asw->getType(boost::get<pHandle>(*outh->begin())), AND_LINK)
			|| outh->begin().number_of_children() != N)
			return Rule::setOfMPs();
		
		tree<Vertex>::iterator top = outh->begin();
		MPs ret;//(outh->begin(top), outh->end(top));

		//for (int i = 0; i < N; i++)
		
		for (tree<Vertex>::sibling_iterator i =outh->begin(top);
									i!=outh->end  (top); i++)
			ret.push_back(BBvtree(new BoundVTree(i)));

		overrideInputFilter = true;
		
		return makeSingletonSet(ret);
	}

	BoundVertex compute(const std::vector<Vertex>& premiseArray, pHandle CX=PHANDLE_UNDEFINED) const
	{
		pHandle *hs = new pHandle[premiseArray.size()];
		transform(premiseArray.begin(), premiseArray.end(), &hs[0], GetHandle()); //mem_fun(
		const int n = (int)premiseArray.size();
		pHandleSeq dummy_outgoing;
		dummy_outgoing.push_back(boost::get<pHandle>(premiseArray[0]));

		//printf("ANDRUle: [%d: %d] %s =>\n", N, v2h(premiseArray[0]), getTruthValue(v2h(premiseArray[0]))->toString().c_str());

/*		puts("ANDRule got args:");
		foreach(const Vertex& v, premiseArray)
			printTree(v2h(v),0,-3);
	*/
		//currentDebugLevel = 3;
		pHandle ret = ((N>1)
			      ? UnorderedCcompute(asw, AND_LINK, fN, hs,n,CX)
			      : asw->addLink(AND_LINK, dummy_outgoing,
                                 asw->getTV(boost::get<pHandle>(premiseArray[0])),
                                 RuleResultFreshness));
		    delete[] hs;

		      //		printf("=> ANDRUle: %s:\n", ret, getTruthValue(ret)->toString().c_str());
		      //		printTree(ret,0,-3);
		      //currentDebugLevel = -3;

		return Vertex(ret);
	}
	
	NO_DIRECT_PRODUCTION;
};

}} // namespace opencog { namespace pln {
#endif // SIMPLEANDRULE_H
