/*
 * opencog/atomspace/AtomSpace.h
 *
 * Copyright (C) 2002-2007 Novamente LLC
 * All Rights Reserved
 *
 * Written by Welter Silva <welter@vettalabs.com>
 *            Andre Senna <senna@vettalabs.com>
 *            Carlos Lopes <dlopes@vettalabs.com>
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

#ifndef _OPENCOG_ATOMSPACE_H
#define _OPENCOG_ATOMSPACE_H

#include <algorithm>
#include <list>
#include <set>
#include <vector>

#include <boost/scoped_ptr.hpp>

#include <opencog/atomspace/AtomSpaceAsync.h>
#include <opencog/atomspace/AtomTable.h>
#include <opencog/atomspace/AttentionValue.h>
#include <opencog/atomspace/BackingStore.h>
#include <opencog/atomspace/ClassServer.h>
#include <opencog/atomspace/HandleSet.h>
#include <opencog/atomspace/TimeServer.h>
#include <opencog/atomspace/SpaceServer.h>
#include <opencog/atomspace/TruthValue.h>
#include <opencog/util/exceptions.h>
#include <opencog/util/recent_val.h>


namespace opencog
{

typedef std::vector<HandleSet*> HandleSetSeq;
typedef boost::shared_ptr<TruthValue> TruthValuePtr;

class AtomSpace
{
    friend class SavingLoading;
    friend class ::AtomTableUTest;
    friend class SaveRequest;

    void do_merge_tv(Handle, const TruthValue&);

public:
    AtomSpace(void);
    ~AtomSpace();

    /** The container class will eventually just be a wrapper of the asynchronous
     * AtomSpaceAsync which returns ASRequest "futures". Functions in this
     * class will block until notified that they've been fulfilled by the
     * AtomSpaceAsync event loop.
     */
    mutable AtomSpaceAsync atomSpaceAsync;

    /**
     * Recursively store the atom to the backing store.
     * I.e. if the atom is a link, then store all of the atoms
     * in its outgoing set as well, recursively.
     * @deprecated Use AtomSpaceAsync::storeAtom in new code.
     */
    inline void storeAtom(Handle h) {
        atomSpaceAsync.storeAtom(h)->get_result();
    }

    /**
     * Return the atom with the indicated handle. This method will
     * explicitly use the backing store to obtain an instance of the
     * atom. If an atom corresponding to the handle cannot be found,
     * then an undefined handle is returned. If the atom is found, 
     * then the corresponding atom is guaranteed to have been
     * instantiated in the atomspace.
     * @deprecated Use AtomSpaceAsync::fetchAtom in new code.
     */
    inline Handle fetchAtom(Handle h) {
        return atomSpaceAsync.fetchAtom(h)->get_result();
    }

    /**
     * Use the backing store to load the entire incoming set of the atom.
     * If the flag is true, then the load is done recursively. 
     * This method queries the backing store to obtain all atoms that 
     * contain this one in their outgoing sets. All of these atoms are
     * then loaded into this AtomSpace's AtomTable.
     * @deprecated Use AtomSpaceAsync::fetchIncomingSet in new code.
     */
    inline Handle fetchIncomingSet(Handle h, bool recursive) {
        return atomSpaceAsync.fetchIncomingSet(h,recursive)->get_result();
    };

    /**
     * Get time server associated with the AtomSpace
     * @deprecated Use AtomSpaceAsync::getTimeServer in new code.
     * @return a const reference to the TimeServer object of this AtomSpace
     */
    inline TimeServer& getTimeServer() const {
        return atomSpaceAsync.getTimeServer();
    }

    /**
     * Get space server associated with the AtomSpace
     * @deprecated Use AtomSpaceAsync::getSpaceServer in new code.
     * @return a reference to the SpaceServer object of this AtomSpace
     */
    inline SpaceServer& getSpaceServer() const {
        return atomSpaceAsync.getSpaceServer();
    }

    /**
     * Return the number of atoms contained in the space.
     */
    inline int getSize() const { return atomSpaceAsync.getSize()->get_result(); }

    /**
     * DEPRECATED! Add an atom an optional TruthValue object to the Atom Table
     * This is a deprecated function; do not use it in new code,
     * if at all possible.
     *
     * @param atom the handle of the Atom to be added
     * @param tvn the TruthValue object to be associated to the added
     *        atom. NULL if the own atom's tv must be used.
     * @return Handle referring to atom after it's been added.
     * @deprecated This is a legacy code left-over from when one could
     * have non-real atoms, i.e. those whose handles were
     * less than 500, and indicated types, not atoms.
     * Instead of using that method, one should use
     * addNode or addLink (which is a bit faster too) which is actually called
     * internally by this wrapper.
     */
    Handle addRealAtom(const Atom& atom,
                       const TruthValue& tvn = TruthValue::NULL_TV());

    inline bool saveToXML(const std::string& filename) const {
        return atomSpaceAsync.saveToXML(filename)->get_result();
    }

    /**
     * Prints atoms of this AtomSpace to the given output stream.
     * @param output  the output stream where the atoms will be printed.
     * @param type  the type of atoms that should be printed.
     * @param subclass  if true, matches all atoms whose type is
     *              subclass of the given type. If false, matches
     *              only atoms of the exact type.
     */
    void print(std::ostream& output = std::cout,
               Type type = ATOM, bool subclass = true) const {
        atomSpaceAsync.print(output, type, subclass)->get_result();
    }

    /** Add a new node to the Atom Table,
     * if the atom already exists then the old and the new truth value is merged
     * \param t     Type of the node
     * \param name  Name of the node
     * \param tvn   Optional TruthValue of the node. If not provided, uses the DEFAULT_TV (see TruthValue.h) 
     * @deprecated New code should directly use the AtomSpaceAsync::addNode method.
     */
    inline Handle addNode(Type t, const std::string& name = "", const TruthValue& tvn = TruthValue::DEFAULT_TV())
    {
        return atomSpaceAsync.addNode(t,name,tvn)->get_result();
    }

    /**
     * Add a new link to the Atom Table
     * If the atom already exists then the old and the new truth value
     * is merged
     * @param t         Type of the link
     * @param outgoing  a const reference to a HandleSeq containing
     *                  the outgoing set of the link
     * @param tvn       Optional TruthValue of the node. If not
     *                  provided, uses the DEFAULT_TV (see TruthValue.h)
     * @deprecated New code should directly use the AtomSpaceAsync::addLink method.
     */
    inline Handle addLink(Type t, const HandleSeq& outgoing,
                   const TruthValue& tvn = TruthValue::DEFAULT_TV())
    { 
        return atomSpaceAsync.addLink(t,outgoing,tvn)->get_result();
    }

    inline Handle addLink(Type t, Handle h,
                   const TruthValue& tvn = TruthValue::DEFAULT_TV())
    {
        HandleSeq oset;
        oset.push_back(h);
        return addLink(t, oset, tvn);
    }

    inline Handle addLink(Type t, Handle ha, Handle hb,
                   const TruthValue& tvn = TruthValue::DEFAULT_TV())
    {
        HandleSeq oset;
        oset.push_back(ha);
        oset.push_back(hb);
        return addLink(t, oset, tvn);
    }

    inline Handle addLink(Type t, Handle ha, Handle hb, Handle hc,
                   const TruthValue& tvn = TruthValue::DEFAULT_TV())
    {
        HandleSeq oset;
        oset.push_back(ha);
        oset.push_back(hb);
        oset.push_back(hc);
        return addLink(t, oset, tvn);
    }

    inline Handle addLink(Type t, Handle ha, Handle hb, Handle hc, Handle hd,
                   const TruthValue& tvn = TruthValue::DEFAULT_TV())
    {
        HandleSeq oset;
        oset.push_back(ha);
        oset.push_back(hb);
        oset.push_back(hc);
        oset.push_back(hd);
        return addLink(t, oset, tvn);
    }

    inline Handle addLink(Type t, Handle ha, Handle hb, Handle hc, Handle hd, Handle he,
                   const TruthValue& tvn = TruthValue::DEFAULT_TV())
    {
        HandleSeq oset;
        oset.push_back(ha);
        oset.push_back(hb);
        oset.push_back(hc);
        oset.push_back(hd);
        oset.push_back(he);
        return addLink(t, oset, tvn);
    }

    inline Handle addLink(Type t, Handle ha, Handle hb, Handle hc,
                          Handle hd, Handle he, Handle hf,
                   const TruthValue& tvn = TruthValue::DEFAULT_TV())
    {
        HandleSeq oset;
        oset.push_back(ha);
        oset.push_back(hb);
        oset.push_back(hc);
        oset.push_back(hd);
        oset.push_back(he);
        oset.push_back(hf);
        return addLink(t, oset, tvn);
    }

    inline Handle addLink(Type t, Handle ha, Handle hb, Handle hc,
                          Handle hd, Handle he, Handle hf, Handle hg,
                   const TruthValue& tvn = TruthValue::DEFAULT_TV())
    {
        HandleSeq oset;
        oset.push_back(ha);
        oset.push_back(hb);
        oset.push_back(hc);
        oset.push_back(hd);
        oset.push_back(he);
        oset.push_back(hf);
        oset.push_back(hg);
        return addLink(t, oset, tvn);
    }

    inline Handle addLink(Type t, Handle ha, Handle hb, Handle hc,
                          Handle hd, Handle he, Handle hf, Handle hg,
                          Handle hh,
                   const TruthValue& tvn = TruthValue::DEFAULT_TV())
    {
        HandleSeq oset;
        oset.push_back(ha);
        oset.push_back(hb);
        oset.push_back(hc);
        oset.push_back(hd);
        oset.push_back(he);
        oset.push_back(hf);
        oset.push_back(hg);
        oset.push_back(hh);
        return addLink(t, oset, tvn);
    }

    inline Handle addLink(Type t, Handle ha, Handle hb, Handle hc,
                          Handle hd, Handle he, Handle hf, Handle hg,
                          Handle hh, Handle hi,
                   const TruthValue& tvn = TruthValue::DEFAULT_TV())
    {
        HandleSeq oset;
        oset.push_back(ha);
        oset.push_back(hb);
        oset.push_back(hc);
        oset.push_back(hd);
        oset.push_back(he);
        oset.push_back(hf);
        oset.push_back(hg);
        oset.push_back(hh);
        oset.push_back(hi);
        return addLink(t, oset, tvn);
    }

    /**
     * Removes an atom from the atomspace
     *
     * When the atom is removed from the atomspace, all memory associated
     * with it is also deleted; in particular, the atom is removed from
     * the TLB as well, so that future TLB lookups will be invalid. 
     *
     * @param h The Handle of the atom to be removed.
     * @param recursive Recursive-removal flag; if set, the links in the
     *        incoming set of the atom to be removed will also be
     *        removed.
     * @return True if the Atom for the given Handle was successfully
     *         removed. False, otherwise.
     */
    bool removeAtom(Handle h, bool recursive = false) {
        return atomSpaceAsync.removeAtom(h,recursive)->get_result();
    }

    /**
     * Retrieve from the Atom Table the Handle of a given node
     *
     * @param t     Type of the node
     * @param str   Name of the node
    */
    Handle getHandle(Type t, const std::string& str) const {
        return atomSpaceAsync.getHandle(t,str)->get_result();
    }

    /**
     * Retrieve from the Atom Table the Handle of a given link
     * @param t        Type of the node
     * @param outgoing a reference to a HandleSeq containing
     *        the outgoing set of the link.
    */
    Handle getHandle(Type t, const HandleSeq& outgoing) const {
        return atomSpaceAsync.getHandle(t,outgoing)->get_result();
    }

    /** Get the atom referred to by Handle h represented as a string. */
    std::string atomAsString(Handle h, bool terse = true) const {
        return atomSpaceAsync.atomAsString(h,terse)->get_result();
    }

    /** Retrieve the name of a given Handle */
    std::string getName(Handle h) const {
        return atomSpaceAsync.getName(h)->get_result();
    }

    /** Change the Short-Term Importance of a given Handle */
    void setSTI(Handle h, AttentionValue::sti_t stiValue) {
        atomSpaceAsync.setSTI(h, stiValue)->get_result();
    }

    /** Change the Long-term Importance of a given Handle */
    void setLTI(Handle h, AttentionValue::lti_t ltiValue) {
        atomSpaceAsync.setLTI(h, ltiValue)->get_result();
    }

    /** Change the Very-Long-Term Importance of a given Handle */
    void setVLTI(Handle h, AttentionValue::vlti_t vltiValue) {
        atomSpaceAsync.setVLTI(h, vltiValue)->get_result();
    }

    /** Retrieve the Short-Term Importance of a given Handle */
    AttentionValue::sti_t getSTI(Handle h) const {
        return atomSpaceAsync.getSTI(h)->get_result();
    }

    /** Retrieve the Long-term Importance of a given AttentionValueHolder */
    AttentionValue::lti_t getLTI(Handle h) const {
        return atomSpaceAsync.getLTI(h)->get_result();
    }

    /** Retrieve the Very-Long-Term Importance of a given
     * AttentionValueHolder */
    AttentionValue::vlti_t getVLTI(Handle h) const {
        return atomSpaceAsync.getVLTI(h)->get_result();
    }

    /** Retrieve the outgoing set of a given link */
    HandleSeq getOutgoing(Handle h) const {
        return atomSpaceAsync.getOutgoing(h)->get_result();
    }

    /** Retrieve a single Handle from the outgoing set of a given link */
    Handle getOutgoing(Handle h, int idx) const {
        return atomSpaceAsync.getOutgoing(h,idx)->get_result();
    }

    /** Retrieve the arity of a given link */
    int getArity(Handle h) const {
        return atomSpaceAsync.getArity(h)->get_result();
    }

    /** Return whether s is the source handle in a link l */ 
    bool isSource(Handle source, Handle link) const {
        return atomSpaceAsync.isSource(source, link)->get_result();
    }

    /** Retrieve the AttentionValue of a given Handle */
    AttentionValue getAV(Handle h) const;

    /** Change the AttentionValue of a given Handle */
    void setAV(Handle h, const AttentionValue &av);

    /** Retrieve the type of a given Handle */
    Type getType(Handle h) const {
        return atomSpaceAsync.getType(h)->get_result();
    }

    /** Retrieve the TruthValue of a given Handle
     * @note This is an unpleasant hack due to the TruthValue class being abstract and
     * traditionally callers expected to receive a const reference to an  Atom's TV
     * object. To prevent threads from crashing into one another, we have to
     * return a copy of the object... but the caller needs to clean it up. To
     * avoid inspecting all the code for where to delete the copy, I (Joel)
     * have opted for a smart pointer, in the same way that I did with
     * cloneAtom.
     */
    TruthValuePtr getTV(Handle h, VersionHandle vh = NULL_VERSION_HANDLE) const {
        TruthValueRequest tvr = atomSpaceAsync.getTV(h, vh);
        const TruthValue& result = *tvr->get_result();
        // Need to clone the result's TV as it will be deleted when the request
        // is.
        return TruthValuePtr(result.clone());
    }

    /** Change the TruthValue of a given Handle */
    void setTV(Handle h, const TruthValue& tv, VersionHandle vh = NULL_VERSION_HANDLE) {
        atomSpaceAsync.setTV(h, tv, vh)->get_result();
    }

    /** Change the primary TV's mean of a given Handle
     * @note By Joel: this makes no sense to me, how can you generally set a mean
     * across all TV types. I think a better solution would be to remove this
     * enforce the use of setTV.
     */
    void setMean(Handle h, float mean) {
        atomSpaceAsync.setMean(h, mean)->get_result();
    }

    /** Clone an atom from the TLB, replaces the public access to TLB::getAtom
     * that many modules were doing.
     * @param h Handle of atom to clone
     * @return A smart pointer to the atom
     * @note Any changes to the atom object must be committed using
     * AtomSpace::commitAtom for them to be merged with the AtomSpace.
     * Otherwise changes are lost.
     */
    boost::shared_ptr<Atom> cloneAtom(const Handle h) const;
    boost::shared_ptr<Node> cloneNode(const Handle h) const;
    boost::shared_ptr<Link> cloneLink(const Handle h) const;

    /** Commit an atom that has been cloned from the AtomSpace.
     *
     * @param a Atom to commit
     * @return whether the commit was successful
     */
    bool commitAtom(const Atom& a);

    bool isValidHandle(const Handle h) const;

    // TODO - convert to use async or remove, then move to near the rest of the
    // AV functions
    /** Retrieve the AttentionValue of an attention value holder */
    const AttentionValue& getAV(AttentionValueHolder *avh) const;

    /** Change the AttentionValue of an attention value holder */
    void setAV(AttentionValueHolder *avh, const AttentionValue &av);

    /** Change the Short-Term Importance of an attention value holder */
    void setSTI(AttentionValueHolder *avh, AttentionValue::sti_t);

    /** Change the Long-term Importance of an attention value holder */
    void setLTI(AttentionValueHolder *avh, AttentionValue::lti_t);

    /** Change the Very-Long-Term Importance of an attention value holder */
    void setVLTI(AttentionValueHolder *avh, AttentionValue::vlti_t);

    /** Retrieve the Short-Term Importance of an attention value holder */
    AttentionValue::sti_t getSTI(AttentionValueHolder *avh) const;

    /** Retrieve the Long-term Importance of a given Handle */
    AttentionValue::lti_t getLTI(AttentionValueHolder *avh) const;

    /** Retrieve the Very-Long-Term Importance of a given Handle */
    AttentionValue::vlti_t getVLTI(AttentionValueHolder *avh) const;

    /** Retrieve the doubly normalised Short-Term Importance between -1..1
     * for a given AttentionValueHolder. STI above and below threshold
     * normalised separately and linearly.
     *
     * @param h The attention value holder to get STI for
     * @param average Should the recent average max/min STI be used, or the
     * exact min/max?
     * @param clip Should the returned value be clipped to -1..1? Outside this
     * range can be return if average=true
     * @return normalised STI between -1..1
     */
    float getNormalisedSTI(AttentionValueHolder *avh, bool average=true, bool clip=false) const;

    /** Retrieve the linearly normalised Short-Term Importance between 0..1
     * for a given AttentionValueHolder.
     *
     * @param h The attention value holder to get STI for
     * @param average Should the recent average max/min STI be used, or the
     * exact min/max?
     * @param clip Should the returned value be clipped to 0..1? Outside this
     * range can be return if average=true
     * @return normalised STI between 0..1
     */
    float getNormalisedZeroToOneSTI(AttentionValueHolder *avh, bool average=true, bool clip=false) const;

    /** Retrieve the doubly normalised Short-Term Importance between -1..1
     * for a given Handle. STI above and below threshold normalised separately
     * and linearly.
     *
     * @param h The atom handle to get STI for
     * @param average Should the recent average max/min STI be used, or the
     * exact min/max?
     * @param clip Should the returned value be clipped to -1..1? Outside this
     * range can be return if average=true
     * @return normalised STI between -1..1
     */
    float getNormalisedSTI(Handle h, bool average=true, bool clip=false) const {
        return getNormalisedSTI(TLB::getAtom(h), average, clip);
    }

    /** Retrieve the linearly normalised Short-Term Importance between 0..1
     * for a given Handle.
     *
     * @param h The atom handle to get STI for
     * @param average Should the recent average max/min STI be used, or the
     * exact min/max?
     * @param clip Should the returned value be clipped to 0..1? Outside this
     * range can be return if average=true
     * @return normalised STI between 0..1
     */
    float getNormalisedZeroToOneSTI(Handle h, bool average=true, bool clip=false) const {
        return getNormalisedZeroToOneSTI(TLB::getAtom(h), average, clip);
    }
    //---

    /** Get hash for an atom */
    size_t getAtomHash(const Handle h) const;

    /**
     * Returns neighboring atoms, following links and returning their
     * target sets.
     * @param h Get neighbours for the atom this handle points to.
     * @param fanin Whether directional links point to this node should be
     * considered.
     * @param fanout Whether directional links point from this node to
     * another should be considered.
     * @param linkType Follow only these types of links.
     * @param subClasses Follow subtypes of linkType too.
     */
    HandleSeq getNeighbors(const Handle h, bool fanin, bool fanout,
            Type linkType=LINK, bool subClasses=true) const {
        return atomSpaceAsync.getNeighbors(h,fanin,fanout,linkType,subClasses)->get_result();
    }

    /** Retrieve the incoming set of a given atom */
    HandleSeq getIncoming(Handle h) {
        return atomSpaceAsync.getIncoming(h)->get_result();
    }

    /** Retrieve the Count of a given Handle */
    // this is excessive and not part of core API
    //float getCount(Handle) const;

    /** Returns the default TruthValue */
    //static const TruthValue& getDefaultTV();

    //----type properties - these should be abandoned
    //and accesed through the classserver as they don't have anything
    //to do with handles... at the very least they should be static.
    Type getAtomType(const std::string& typeName) const;
    bool isNode(const Type t) const;
    bool isLink(const Type t) const;
    /** Does t1 inherit from t2 */
    bool inheritsType(Type t1, Type t2) const;
    // Get type name 
    std::string getName(Type t) const;
    //-----

    /** Convenience functions... */
    bool isNode(const Handle& h) const;
    bool isLink(const Handle& h) const;

    /**
     * Gets a set of handles that matches with the given arguments.
     *
     * @param result An output iterator.
     * @param type the type of the atoms to be searched
     * @param name the name of the atoms to be searched.
     *        For searching only links, use "" or a search by type.
     * @param subclass if sub types of the given type are accepted in this search
     * @param vh only atoms that contains versioned TVs with
     *        the given VersionHandle are returned. If NULL_VERSION_HANDLE is given,
     *        it does not restrict the result.
     *
     * @return The set of atoms of a given type (subclasses optionally).
     *
     * @note The matched entries are appended to a container whose
     * OutputIterator is passed as the first argument. Example of a
     * call to this method, which would return all entries in AtomSpace:
     * @code
     *      std::list<Handle> ret;
     *      atomSpace.getHandleSet(back_inserter(ret), ATOM, true);
     * @endcode
     */
    template <typename OutputIterator> OutputIterator
    getHandleSet(OutputIterator result,
                 Type type,
                 const std::string& name,
                 bool subclass = true,
                 VersionHandle vh = NULL_VERSION_HANDLE) const {
        HandleSeq result_set = atomSpaceAsync.getHandlesByName(
                std::string(name), type, subclass, vh)->get_result();
        return toOutputIterator(result, result_set);
    }

    /**
     * Returns the set of atoms of a given name (atom type and subclasses
     * optionally).
     *
     * @param result An output iterator.
     * @param name The desired name of the atoms.
     * @param type The type of the atom.
     * @param subclass Whether atom type subclasses should be considered.
     * @param vh only atoms that contains versioned TVs with the given VersionHandle are returned.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     * @return The set of atoms of the given type and name.
     *
     * @note The matched entries are appended to a container whose
     * OutputIterator is passed as the first argument.
     *
     * @note Example of call to this method, which would return all entries in AtomSpace:
     * @code
     *         std::list<Handle> ret;
     *         atomSpace.getHandleSet(back_inserter(ret), ATOM, true);
     * @endcode
     */
    template <typename OutputIterator> OutputIterator
    getHandleSet(OutputIterator result,
                 const char* name,
                 Type type,
                 bool subclass = true,
                 VersionHandle vh = NULL_VERSION_HANDLE) const {
        HandleSeq result_set = atomSpaceAsync.getHandlesByName(
                name, type, subclass, vh)->get_result();
        return toOutputIterator(result, result_set);
    }

    /**
     * Gets a set of handles that matches with the given type
     * (subclasses optionally).
     *
     * @param result An output iterator.
     * @param type The desired type.
     * @param subclass Whether type subclasses should be considered.
     * @param vh only atoms that contains versioned TVs with the given VersionHandle are returned.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     *
     * @return The set of atoms of a given type (subclasses optionally).
     *
     * @note The matched entries are appended to a container whose OutputIterator is passed as the first argument.
     *          Example of call to this method, which would return all entries in AtomSpace:
     * @code
     *         std::list<Handle> ret;
     *         atomSpace.getHandleSet(back_inserter(ret), ATOM, true);
     * @endcode
     */
    template <typename OutputIterator> OutputIterator
    getHandleSet(OutputIterator result,
                 Type type,
                 bool subclass = false,
                 VersionHandle vh = NULL_VERSION_HANDLE) const {
        HandleSeq result_set = atomSpaceAsync.getHandlesByType(type, subclass, vh)->get_result();
        return toOutputIterator(result, result_set);
    }

    /**
     * Returns the set of atoms of a given type which have atoms of a
     * given target type in their outgoing set (subclasses optionally).
     *
     * @param result An output iterator.
     * @param type The desired type.
     * @param targetType The desired target type.
     * @param subclass Whether type subclasses should be considered.
     * @param targetSubclass Whether target type subclasses should be considered.
     * @param vh only atoms that contains versioned TVs with the given VersionHandle are returned.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     * @param targetVh only atoms whose target contains versioned TVs with the given VersionHandle are returned.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     * @return The set of atoms of a given type and target type (subclasses
     * optionally).
     *
     * @note The matched entries are appended to a container whose OutputIterator is passed as the first argument.
     *          Example of call to this method, which would return all entries in AtomSpace:
     * @code
     *         std::list<Handle> ret;
     *         atomSpace.getHandleSet(back_inserter(ret), ATOM, true);
     * @endcode
     */
    template <typename OutputIterator> OutputIterator
    getHandleSet(OutputIterator result,
                 Type type,
                 Type targetType,
                 bool subclass,
                 bool targetSubclass,
                 VersionHandle vh = NULL_VERSION_HANDLE,
                 VersionHandle targetVh = NULL_VERSION_HANDLE) const {
        HandleSeq result_set = atomSpaceAsync.getHandlesByTarget(type, targetType,
                subclass, targetSubclass, vh, targetVh)->get_result();
        return toOutputIterator(result, result_set);
    }

    /**
     * Returns the set of atoms with a given target handle in their
     * outgoing set (atom type and its subclasses optionally).
     *
     * @param result An output iterator.
     * @param handle The handle that must be in the outgoing set of the atom.
     * @param type The type of the atom.
     * @param subclass Whether atom type subclasses should be considered.
     * @param vh only atoms that contains versioned TVs with the given VersionHandle.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     * @return The set of atoms of the given type with the given handle in
     * their outgoing set.
     *
     * @note The matched entries are appended to a container whose OutputIterator is passed as the first argument.
     *          Example of call to this method, which would return all entries in AtomSpace:
     * @code
     *         std::list<Handle> ret;
     *         atomSpace.getHandleSet(back_inserter(ret), ATOM, true);
     * @endcode
     */
    template <typename OutputIterator> OutputIterator
    getHandleSet(OutputIterator result,
                 Handle handle,
                 Type type,
                 bool subclass,
                 VersionHandle vh = NULL_VERSION_HANDLE) const {
        HandleSeq result_set = atomSpaceAsync.getHandlesByTargetHandle(handle,
                type, subclass, vh)->get_result();
        return toOutputIterator(result, result_set);
    }

    /**
     * Returns the set of atoms with the given target handles and types
     * (order is considered) in their outgoing sets, where the type and
     * subclasses of the atoms are optional.
     *
     * @param result An output iterator.
     * @param handles An array of handles to match the outgoing sets of the searched
     * atoms. This array can be empty (or each of its elements can be null), if
     * the handle value does not matter or if it does not apply to the
     * specific search.
     * Note that if this array is not empty, it must contain "arity" elements.
     * @param types An array of target types to match the types of the atoms in
     * the outgoing set of searched atoms.
     * @param subclasses An array of boolean values indicating whether each of the
     * above types must also consider subclasses. This array can be null,
     * what means that subclasses will not be considered. Note that if this
     * array is not null, it must contains "arity" elements.
     * @param arity The length of the outgoing set of the atoms being searched.
     * @param type The type of the atom.
     * @param subclass Whether atom type subclasses should be considered.
     * @param vh only atoms that contains versioned TVs with the given VersionHandle are returned.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     * @return The set of atoms of the given type with the matching
     * criteria in their outgoing set.
     *
     * @note The matched entries are appended to a container whose OutputIterator
     * is passed as the first argument. Example of call to this method, which
     * would return all entries in AtomSpace:
     * @code
     *     std::list<Handle> ret;
     *     atomSpace.getHandleSet(back_inserter(ret), ATOM, true);
     * @endcode
     */
    template <typename OutputIterator> OutputIterator
    getHandleSet(OutputIterator result,
                 const HandleSeq& handles,
                 Type* types,
                 bool* subclasses,
                 Arity arity,
                 Type type,
                 bool subclass,
                 VersionHandle vh = NULL_VERSION_HANDLE) const {
        HandleSeq result_set = atomSpaceAsync.getHandlesByOutgoingSet(
                handles,types,subclasses,arity,type,subclass,vh)->get_result();
        return toOutputIterator(result, result_set);
    }

    /**
     * Returns the set of atoms whose outgoing set contains at least one
     * atom with the given name and type (atom type and subclasses
     * optionally).
     *
     * @param result An output iterator.
     * @param targetName The name of the atom in the outgoing set of the searched
     * atoms.
     * @param targetType The type of the atom in the outgoing set of the searched
     * atoms.
     * @param type type of the atom.
     * @param subclass Whether atom type subclasses should be considered.
     * @param vh return only atoms that contains versioned TVs with the given VersionHandle.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     * @return The set of atoms of the given type and name whose outgoing
     * set contains at least one atom of the given type and name.
     *
     * @note The matched entries are appended to a container whose OutputIterator is passed as the first argument.
     *          Example of call to this method, which would return all entries in AtomSpace:
     * @code
     *         std::list<Handle> ret;
     *         atomSpace.getHandleSet(back_inserter(ret), ATOM, true);
     * @endcode
     */
    template <typename OutputIterator> OutputIterator
    getHandleSet(OutputIterator result,
                 const char* targetName,
                 Type targetType,
                 Type type,
                 bool subclass,
                 VersionHandle vh = NULL_VERSION_HANDLE,
                 VersionHandle targetVh = NULL_VERSION_HANDLE) const {
        HandleSeq result_set = atomSpaceAsync.getHandlesByTargetName(
               targetName, targetType, type, subclass, vh, targetVh)->get_result();
        return toOutputIterator(result, result_set);
    }

    /**
     * Returns the set of atoms with the given target names and/or types
     * (order is considered) in their outgoing sets, where the type
     * and subclasses arguments of the searched atoms are optional.
     *
     * @param result An output iterator.
     * @param names An array of names to match the outgoing sets of the searched
     * atoms. This array (or each of its elements) can be null, if
     * the names do not matter or if do not apply to the specific search.
     * Note that if this array is not null, it must contain "arity" elements.
     * @param types An array of target types to match the types of the atoms in
     * the outgoing set of searched atoms. If array of names is not null,
     * this parameter *cannot* be null as well. Besides, if an element in a
     * specific position in the array of names is not null, the corresponding
     * type element in this array *cannot* be NOTYPE as well.
     * @param subclasses An array of boolean values indicating whether each of the
     * above types must also consider subclasses. This array can be null,
     * what means that subclasses will not be considered. Not that if this
     * array is not null, it must contains "arity" elements.
     * @param arity The length of the outgoing set of the atoms being searched.
     * @param type The optional type of the atom.
     * @param subclass Whether atom type subclasses should be considered.
     * @param vh return only atoms that contains versioned TVs with the given VersionHandle.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     * @return The set of atoms of the given type with the matching
     * criteria in their outgoing set.
     *
     * @note The matched entries are appended to a container whose OutputIterator is passed as the first argument.
     *          Example of call to this method, which would return all entries in AtomSpace:
     * @code
     *         std::list<Handle> ret;
     *         atomSpace.getHandleSet(back_inserter(ret), ATOM, true);
     * @endcode
     */
    template <typename OutputIterator> OutputIterator
    getHandleSet(OutputIterator result,
                 const char** names,
                 Type* types,
                 bool* subclasses,
                 Arity arity,
                 Type type,
                 bool subclass,
                 VersionHandle vh = NULL_VERSION_HANDLE) const {

        HandleSeq result_set = atomSpaceAsync.getHandlesByTargetNames(
                names, types, subclasses, arity, type, subclass, vh)->get_result();
        return toOutputIterator(result, result_set);
    }

    /**
     * Returns the set of atoms with the given target names and/or types
     * (order is considered) in their outgoing sets, where the type
     * and subclasses arguments of the searched atoms are optional.
     *
     * @param result An output iterator.
     * @param types An array of target types to match the types of the atoms in
     * the outgoing set of searched atoms. This parameter can be null (or any of
     * its elements can be NOTYPE), what means that the type doesnt matter.
     * Not that if this array is not null, it must contains "arity" elements.
     * @param subclasses An array of boolean values indicating whether each of the
     * above types must also consider subclasses. This array can be null,
     * what means that subclasses will not be considered. Not that if this
     * array is not null, it must contains "arity" elements.
     * @param arity The length of the outgoing set of the atoms being searched.
     * @param type The optional type of the atom.
     * @param subclass Whether atom type subclasses should be considered.
     * @param vh returns only atoms that contains versioned TVs with the given VersionHandle.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     * @return The set of atoms of the given type with the matching
     * criteria in their outgoing set.
     *
     * @note The matched entries are appended to a container whose OutputIterator is passed as the first argument.
     *          Example of call to this method, which would return all entries in AtomSpace:
     * @code
     *         std::list<Handle> ret;
     *         atomSpace.getHandleSet(back_inserter(ret), ATOM, true);
     * @endcode
     */
    template <typename OutputIterator> OutputIterator
    getHandleSet(OutputIterator result,
                 Type* types,
                 bool* subclasses,
                 Arity arity,
                 Type type,
                 bool subclass,
                 VersionHandle vh = NULL_VERSION_HANDLE) const {

        HandleSeq result_set = atomSpaceAsync.getHandlesByTargetTypes(
                types, subclasses, arity, type, subclass, vh)->get_result();
        return toOutputIterator(result, result_set);
    }

    /**
     * Gets a set of handles in the Attentional Focus that matches with the given type
     * (subclasses optionally).
     *
     * @param result An output iterator.
     * @param type The desired type.
     * @param subclass Whether type subclasses should be considered.
     * @param vh returns only atoms that contains versioned TVs with the given VersionHandle.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     *
     * @return The set of atoms of a given type (subclasses optionally).
     *
     * @note The matched entries are appended to a container whose OutputIterator is passed as the first argument.
     *          Example of call to this method, which would return all entries in AtomSpace in the AttentionalFocus:
     * @code
     *         std::list<Handle> ret;
     *         atomSpace.getHandleSet(back_inserter(ret), ATOM, true);
     * @endcode
     */
    template <typename OutputIterator> OutputIterator
    getHandleSetInAttentionalFocus(OutputIterator result,
                 Type type,
                 bool subclass,
                 VersionHandle vh = NULL_VERSION_HANDLE) const
    {
        return getHandleSetFiltered(result, type, subclass, &STIAboveThreshold(getAttentionalFocusBoundary()), vh);

    }

    /**
     * Gets a set of handles that matches with the given type
     * (subclasses optionally) and a given criterion.
     *
     * @param result An output iterator.
     * @param type The desired type.
     * @param subclass Whether type subclasses should be considered.
     * @param compare A criterion for including atoms. It must be something that returns a bool when called.
     * @param vh returns only atoms that contains versioned TVs with the given VersionHandle.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     *
     * @return The set of atoms of a given type (subclasses optionally).
     *
     * @note The matched entries are appended to a container whose OutputIterator is passed as the first argument.
     *          Example of call to this method, which would return all entries in AtomSpace beyond 500 LTI:
     * @code
     *         std::list<Handle> ret;
     *         atomSpace.getHandleSet(back_inserter(ret), ATOM, true, LTIAboveThreshold(500));
     * @endcode
     */
    template <typename OutputIterator> OutputIterator
    getHandleSetFiltered(OutputIterator result,
                 Type type,
                 bool subclass,
                 AtomPredicate* compare,
                 VersionHandle vh = NULL_VERSION_HANDLE) const {
        HandleSeq hs = atomSpaceAsync.filter(compare, type, subclass, vh)->get_result();
        return toOutputIterator(result, hs);
    }

    /**
     * Gets a set of handles that matches with the given type
     * (subclasses optionally), sorted according to the given comparison
     * structure.
     *
     * @param result An output iterator.
     * @param type The desired type.
     * @param subclass Whether type subclasses should be considered.
     * @param compare The comparison struct to use in the sort.
     * @param vh returns only atoms that contains versioned TVs with the given VersionHandle.
     *        If NULL_VERSION_HANDLE is given, it does not restrict the result.
     *
     * @return The set of atoms of a given type (subclasses optionally).
     *
     * @note The matched entries are appended to a container whose OutputIterator is passed as the first argument.
     *          Example of call to this method, which would return all entries in AtomSpace, sorted by STI:
     * @code
     *         std::list<Handle> ret;
     *         AttentionValue::STISort stiSort;
     *         atomSpace.getHandleSet(back_inserter(ret), ATOM, true, stiSort);
     * @endcode
     */
    template <typename OutputIterator, typename Compare> OutputIterator
    getSortedHandleSet(OutputIterator result,
                 Type type,
                 bool subclass,
                 Compare compare,
                 VersionHandle vh = NULL_VERSION_HANDLE) const {
        HandleSeq result_set = atomSpaceAsync.getSortedHandleSet(
                type, subclass, compare, vh)->get_result();
        return toOutputIterator(result, result_set);
    }


    /* ----------------------------------------------------------- */
    /* The foreach routines offer an alternative interface
     * to the getHandleSet API.
     */
    /**
     * Invoke the callback on each handle of the given type.
     */
    template<class T>
    inline bool foreach_handle_of_type(Type atype,
                                       bool (T::*cb)(Handle), T *data,
                                       bool subclass = false) {
        std::list<Handle> handle_set;
        // The intended signatue is
        // getHandleSet(OutputIterator result, Type type, bool subclass)
        getHandleSet(back_inserter(handle_set), atype, subclass);

        // Loop over all handles in the handle set.
        std::list<Handle>::iterator i;
        for (i = handle_set.begin(); i != handle_set.end(); i++) {
            Handle h = *i;
            bool rc = (data->*cb)(h);
            if (rc) return rc;
        }
        return false;
    }

    template<class T>
    inline bool foreach_handle_of_type(const char * atypename,
                                       bool (T::*cb)(Handle), T *data,
                                       bool subclass = false) {
        Type atype = classserver().getType(atypename);
        return foreach_handle_of_type(atype, cb, data, subclass);
    }

    /* ----------------------------------------------------------- */

    /**
     * Decays STI of all atoms (one cycle of importance decay).
     * Deprecated, importance updating should be done by ImportanceUpdating
     * Agent. Still used by Embodiment.
     */
    void decayShortTermImportance();

    /**
     * Get the total amount of STI in the AtomSpace, sum of
     * STI across all atoms.
     *
     * @return total STI in AtomSpace
     */
    long getTotalSTI() const;

    /**
     * Get the total amount of LTI in the AtomSpace, sum of
     * all LTI across atoms.
     *
     * @return total LTI in AtomSpace
     */
    long getTotalLTI() const;

    /**
     * Get the STI funds available in the AtomSpace pool.
     *
     * @return STI funds available
     */
    long getSTIFunds() const;

    /**
     * Get the LTI funds available in the AtomSpace pool.
     *
     * @return LTI funds available
     */
    long getLTIFunds() const;

    /**
     * Get attentional focus boundary, generally atoms below
     * this threshold won't be accessed unless search methods
     * are unsuccessful on those that are above this value.
     *
     * @return Short Term Importance threshold value
     */
    AttentionValue::sti_t getAttentionalFocusBoundary() const;

    /**
     * Change the attentional focus boundary. Some situations
     * may benefit from less focussed searches.
     *
     * @param s New threshold
     * @return Short Term Importance threshold value
     */
    AttentionValue::sti_t setAttentionalFocusBoundary(
        AttentionValue::sti_t s);

    /**
     * Get the maximum STI observed in the AtomSpace.
     *
     * @param average If true, return an exponentially decaying average of
     * maximum STI, otherwise return the actual maximum.
     * @return Maximum STI
     */
    AttentionValue::sti_t getMaxSTI(bool average=true) const;

    /**
     * Get the minimum STI observed in the AtomSpace.
     *
     * @param average If true, return an exponentially decaying average of
     * minimum STI, otherwise return the actual maximum.
     * @return Minimum STI
     */
    AttentionValue::sti_t getMinSTI(bool average=true) const;

    /**
     * Update the minimum STI observed in the AtomSpace. Min/max are not updated
     * on setSTI because average is calculate by lobe cycle, although this could
     * potentially also be handled by the cogServer.
     *
     * @warning Should only be used by attention allocation system.
     * @param m New minimum STI
     */
    void updateMinSTI(AttentionValue::sti_t m);

    /**
     * Update the maximum STI observed in the AtomSpace. Min/max are not updated
     * on setSTI because average is calculate by lobe cycle, although this could
     * potentially also be handled by the cogServer.
     *
     * @warning Should only be used by attention allocation system.
     * @param m New maximum STI
     */
    void updateMaxSTI(AttentionValue::sti_t m);

    // For convenience
    // bool isNode(Handle) const;
    bool isVar(Handle) const;
    bool isList(Handle) const;
    bool containsVar(Handle) const;

    Handle createHandle(Type t, const std::string& str, bool managed = false);
    Handle createHandle(Type t, const HandleSeq& outgoing, bool managed = false);

    int Nodes(VersionHandle = NULL_VERSION_HANDLE) const;
    int Links(VersionHandle = NULL_VERSION_HANDLE) const;

    bool containsVersionedTV(Handle h, VersionHandle vh) const;

    //! Clear the atomspace, remove all atoms
    void clear();

// ---- filter templates

    HandleSeq filter(AtomPredicate* compare, VersionHandle vh = NULL_VERSION_HANDLE) {
        return atomSpaceAsync.filter(compare,ATOM,true,vh)->get_result();
    }

    template<typename OutputIterator>
    OutputIterator filter(OutputIterator it, AtomPredicate* compare, VersionHandle vh = NULL_VERSION_HANDLE) {
        HandleSeq result = atomSpaceAsync.filter(compare,ATOM,true,vh)->get_result();
        foreach(Handle h, result) 
            * it++ = h;
        return it;
    }

    /**
     * Filter handles from a sequence according to the given criterion.
     *
     * @param begin iterator for the sequence
     * @param end iterator for the sequence
     * @param struct or function embodying the criterion
     * @return The handles in the sequence that match the criterion.
     */
    template<typename InputIterator>
    HandleSeq filter(InputIterator begin, InputIterator end, AtomPredicate* compare) const {
        HandleSeq result;
        for (; begin != end; begin++) {
            //std::cout << "evaluating atom " << atomAsString(*begin) << std::endl;
            if ((*compare)(*cloneAtom(*begin))) {
                //std::cout << "passed! " <<  std::endl;
                result.push_back(*begin);
            }
        }
        return result;
    }

    template<typename InputIterator, typename OutputIterator>
    OutputIterator filter(InputIterator begin, InputIterator end, OutputIterator it, AtomPredicate* compare) const {
        for (; begin != end; begin++)
            if (compare(*begin))
                * it++ = *begin;
        return it;
    }

// ---- custom filter templates

    struct TruePredicate : public AtomPredicate {
        TruePredicate(){}
        virtual bool test(const Atom& atom) { return true; }
    };

    // redundant with getHandlesByType...
    //HandleSeq filter_type(Type t, VersionHandle vh = NULL_VERSION_HANDLE) {
    //    return atomSpaceAsync.filter<TruePredicate>(TruePredicate(),t,true,vh)->get_result();
    //}

    // redundant with getHandlesByType...
    //template<typename OutputIterator>
    //OutputIterator filter_type(OutputIterator it, Type type, VersionHandle vh = NULL_VERSION_HANDLE) {
        //HandleSeq result = atomSpaceAsync.filter(TruePredicate(),type,true,vh);
        //foreach(Handle h, result) 
            //* it++ = h;
        //return it;
    //}

    template<typename InputIterator>
    HandleSeq filter_InAttentionalFocus(InputIterator begin, InputIterator end) const {
        STIAboveThreshold sti_above(getAttentionalFocusBoundary());
        return filter(begin, end, &sti_above);
    }

    struct STIAboveThreshold : public AtomPredicate {
        STIAboveThreshold(const AttentionValue::sti_t t) : threshold (t) {}

        virtual bool test(const Atom& atom) {
            return atom.getAttentionValue().getSTI() > threshold;
        }
        AttentionValue::sti_t threshold;
    };

    struct LTIAboveThreshold : public AtomPredicate {
        LTIAboveThreshold(const AttentionValue::lti_t t) : threshold (t) {}

        virtual bool test(const Atom& atom) {
            return atom.getAttentionValue().getLTI() > threshold;
        }
        AttentionValue::lti_t threshold;
    };

protected:

    /*HandleIterator* _handle_iterator;
    TypeIndex::iterator type_itr;
    // these methods are used by the filter_* templates
    void _getNextAtomPrepare();
    Handle _getNextAtom();
    void _getNextAtomPrepare_type(Type type);
    Handle _getNextAtom_type(Type type);*/

private:

    std::string emptyName;

    /* Boundary at which an atom is considered within the attentional
     * focus of opencog. Atom's with STI less than this value are
     * not charged STI rent */
    AttentionValue::sti_t attentionalFocusBoundary;

    opencog::recent_val<AttentionValue::sti_t> maxSTI;
    opencog::recent_val<AttentionValue::sti_t> minSTI;

    /* These indicate the amount importance funds available in the
     * AtomSpace */
    long fundsSTI;
    long fundsLTI;

    /*
     * Remove stimulus from atom, only should be used when Atom is deleted.
     */
    void removeStimulus(Handle h);

    /* copy HandleSeq to an output iterator */
    template <typename OutputIterator> OutputIterator
    toOutputIterator(OutputIterator result, HandleSeq handles) const {
        foreach(Handle h, handles) {
            *(result++) = h;
        }
        return result;
    }

    /* copy HandleEntry to an output iterator */
    template <typename OutputIterator> OutputIterator
    toOutputIterator(OutputIterator result, HandleEntry * handleEntry) const {

        HandleEntry * toRemove = handleEntry;
        while (handleEntry) {
            *(result++) = handleEntry->handle;
            handleEntry = handleEntry->next;
        }
        // free memory
        if (toRemove) delete toRemove;
        return result;
    }

    /**
     * Creates the space map node, if not created yet.
     * returns the handle of the node.
     */
    Handle getSpaceMapNode(void);

public:
    // SpaceServerContainer virtual methods:
    void mapRemoved(Handle mapId);
    void mapPersisted(Handle mapId);
    std::string getMapIdString(Handle mapId);

    /**
     * Overrides and declares copy constructor and equals operator as private 
     * for avoiding large object copying by mistake.
     */
    AtomSpace& operator=(const AtomSpace&);
    AtomSpace(const AtomSpace&);

};

} // namespace opencog

#endif // _OPENCOG_ATOMSPACE_H
