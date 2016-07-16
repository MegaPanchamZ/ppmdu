#include "ssb.hpp"
#include <ppmdu/pmd2/pmd2_scripts.hpp>
#include <ppmdu/pmd2/pmd2_scripts_opcodes.hpp>
#include <utils/utility.hpp>
#include <utils/library_wide.hpp>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
using namespace pmd2;
using namespace std;

namespace filetypes
{

    /*
        PrepareParameterValue
            Fun_022E48AC
            Used on 16 bits words passed to some functions, to turn them into 14 bits words
    */
    int16_t PrepareParameterValue( int32_t R0 )
    {
        int32_t R1 = 0;
        if( (R0 & 0x4000) != 0 )
        {
            R1 = 0x8000;
            R1 = 0 - R1;
            R1 = R0 | R1;
        }
        else
        {
            R1 = 0x3FFF;
            R1 = R0 & R1;
        }

        if( (R0 & 0x8000) != 0 )
        {
            R0 = R1 >> 7;
            R0 = R1 + static_cast<uint32_t>(static_cast<uint32_t>(R0) >> 0x18u);
            R0 = R0 >> 8;
        }
        else
        {
            R0 = R1;
        }
        return  static_cast<int16_t>(R0);
    }

    /*
        Fun_022E48E0 
            Used on the first parameter of the MovePositionMark among other things. 
    */
    int16_t Fun_022E48E0( int32_t R0 )
    {
        int32_t R1 = 0;
        if( (R0 & 0x4000) != 0 )
            R1 = R0 | (0 - 0x8000);
        else
            R1 = R0 & 0x3FFF;
    
        if( (R0 & 0x8000) != 0 )
            R0 = R1;
        else
            R0 = (R1 << 8);
        return static_cast<int16_t>(R0);
    }


    /*
        group_entry
            Script instruction group entry
    */
    struct group_entry
    {
        static const size_t LEN = 3 * sizeof(uint16_t);
        uint16_t begoffset = 0;
        uint16_t type      = 0;
        uint16_t unk2      = 0;

        template<class _outit>
            _outit WriteToContainer(_outit itwriteto)const
        {
            itwriteto = utils::WriteIntToBytes(begoffset,  itwriteto);
            itwriteto = utils::WriteIntToBytes(type,       itwriteto);
            itwriteto = utils::WriteIntToBytes(unk2,       itwriteto);
            return itwriteto;
        }

        //
        template<class _init>
            _init ReadFromContainer(_init itReadfrom, _init itpastend)
        {
            itReadfrom = utils::ReadIntFromBytes(begoffset, itReadfrom, itpastend );
            itReadfrom = utils::ReadIntFromBytes(type,      itReadfrom, itpastend );
            itReadfrom = utils::ReadIntFromBytes(unk2,      itReadfrom, itpastend );
            return itReadfrom;
        }
    };

    /*
        group_bounds
            Helper to contain the bounds of a group
    */
    struct group_bounds
    {
        size_t datoffbeg;
        size_t datoffend; //One past the end of the group
    };

    typedef uint32_t                            dataoffset_t;   //Offset in bytes within the "Data" block of the script
    typedef uint16_t                            lbl_t;          //Represents a label
    typedef unordered_map<dataoffset_t, lbl_t>  lbltbl_t;   //Contains the location of all labels

//=======================================================================================
//  ScriptProcessor
//=======================================================================================
    /*
        ScriptProcessor
            Process script data from a SSB file into a more manageable format
            with several meta-instructions.
    */
    class ScriptProcessor
    {
    public:

        ScriptProcessor( const deque<ScriptInstruction>         & instructions, 
                         const vector<group_entry>              & groups, 
                         const lbltbl_t                         & labellist,
                         const ScriptedSequence::consttbl_t     & constants,
                         const ScriptedSequence::strtblset_t    & strings,
                         size_t                                   datablocklen,
                         eOpCodeVersion                           opver )
            :m_rawinst(instructions), m_groups(groups), m_labels(labellist), 
            m_constants(constants), m_strings(strings),m_curdataoffset(0),m_curgroup(0),
            m_poutgrp(nullptr), m_datablocklen(datablocklen),m_instfinder(opver)
        {}

        void operator()( ScriptedSequence & destseq )
        {
            m_poutgrp  = std::addressof( destseq.Groups() );
            m_grptblandathdrlen = ssb_data_hdr::LEN + (group_entry::LEN * m_groups.size());
            m_curgroup      = 0;
            m_curdataoffset = m_grptblandathdrlen;
            PrepareGroups();

            //Iterate the raw instructions
            auto itend = m_rawinst.end();
            for( auto iti = m_rawinst.begin(); iti != itend; )
            {
                //const auto & inst = *iti;
                UpdateCurrentGroup();
                HandleLabels();
                HandleInstruction( iti, itend );
            }
        }

    private:

        //Helper to access the current group
        inline ScriptInstrGrp & CurGroup()
        {
            return (*m_poutgrp)[m_curgroup];
        }

        //Init destination groups, and make a list of all the end offsets for each groups
        void PrepareGroups()
        {
            m_grpends.reserve(m_groups.size());

            size_t cntgrp = 0;
            for( ; cntgrp <  m_groups.size(); ++cntgrp )
            {
                const auto & grp = m_groups[cntgrp];
                ScriptInstrGrp igrp;
                igrp.type = grp.type;
                igrp.unk2 = grp.unk2;
                m_poutgrp->push_back(std::move(igrp));

                //group_bounds bnds;
                //bnds.datoffbeg = grp.begoffset * ScriptWordLen;
                size_t endoffset = ((cntgrp +1) < m_groups.size())?
                                   (m_groups[cntgrp+1].begoffset * ScriptWordLen) :  
                                   m_datablocklen;
                m_grpends.push_back(endoffset);
            }
        }

        inline void HandleLabels()
        {
            auto itf = m_labels.find(m_curdataoffset);
            if( itf != m_labels.end() )
            {
                ScriptInstruction label;
                label.type  = eInstructionType::MetaLabel;
                label.value = itf->second;
                //Insert label
                CurGroup().instructions.push_back(std::move(label));
            }
        }

        template<typename _init>
            void HandleInstruction( _init & iti, _init & itend )
        {
            const ScriptInstruction & curinst  = *iti;
            OpCodeInfoWrapper         codeinfo = m_instfinder(curinst.value);

            switch(codeinfo.Category())
            {
                case eCommandCat::ProcSpec:
                {
                    HandleProcessSpecial(iti, itend, curinst);
                    break;
                }
                case eCommandCat::Switch:
                {
                    HandleSwitchCommand(iti, itend, curinst);
                    break;
                }
                case eCommandCat::EntityAccessor:
                {
                    HandleAccessor(iti, itend, curinst);
                    break;
                }
                default:
                {
                    CurGroup().instructions.push_back(curinst);
                    m_curdataoffset += GetInstructionLen(curinst);
                    ++iti;
                }
            };
        }

        template<typename _init>
            void HandleProcessSpecial(_init & iti, _init & itend, const ScriptInstruction & curinst)
        {
            HandleCaseOwningCommand<eInstructionType::MetaProcSpecRet>(iti,itend,curinst);
        }

        template<typename _init>
            inline void HandleSwitchCommand(_init & iti, _init & itend, const ScriptInstruction & curinst)
        {
            HandleCaseOwningCommand<eInstructionType::MetaSwitch>(iti,itend,curinst);
        }

        template<eInstructionType _MetaType, typename _init>
            void HandleCaseOwningCommand(_init & iti, _init & itend, const ScriptInstruction & curinst)
        {
            ScriptInstruction outinst = curinst;
            size_t            totalsz = GetInstructionLen(curinst);
            OpCodeInfoWrapper codeinfo;
            
            //#1 Grab all "Case" that are right after
            bool iscase = false;
            do
            {
                ++iti;
                if( iti != itend )
                {
                    codeinfo = m_instfinder(iti->value);
                    iscase   = (codeinfo.Category() == eCommandCat::Case || codeinfo.Category() == eCommandCat::Default); 
                    if(iscase)
                    {
                        outinst.subinst.push_back(*iti);
                        totalsz += GetInstructionLen(*iti);
                    }
                }

            }while( iti != itend && iscase );
               
            if( !(outinst.subinst.empty()) )
                outinst.type = _MetaType;

            CurGroup().instructions.push_back(std::move(outinst));
            m_curdataoffset += totalsz;
        }

        template<typename _init>
            void HandleAccessor(_init & iti, _init & itend, const ScriptInstruction & curinst)
        {
            ScriptInstruction outinst = curinst;
            size_t            totalsz = GetInstructionLen(curinst);
            OpCodeInfoWrapper codeinfo;

            ++iti;
            if( iti != itend )
            {
                codeinfo = m_instfinder(iti->value);
                if( codeinfo && codeinfo.Category() == eCommandCat::EntAttribute )
                {
                    outinst.type = eInstructionType::MetaAccessor;
                    outinst.subinst.push_back(*iti);
                    totalsz += GetInstructionLen(*iti);
                    ++iti;
                }
            }
            CurGroup().instructions.push_back(std::move(outinst));
            m_curdataoffset += totalsz;
        }

        inline size_t GetInstructionLen( const ScriptInstruction & inst )
        {
            if( inst.type == eInstructionType::Command || inst.type == eInstructionType::Data )
                return ScriptWordLen + (inst.parameters.size() * ScriptWordLen);
            else
                return 0;
        }

        inline void UpdateCurrentGroup()
        {
            if( m_curdataoffset >= m_grpends[m_curgroup] && m_curgroup < (m_grpends.size()-1) )
            {
                ++m_curgroup;
            }
            else if( m_curgroup >= m_grpends.size() )
                clog <<"ScriptProcessor::UpdateCurrentGroup() : Instruction is out of expected bounds!\n";
        }

    private:
        const deque<ScriptInstruction>      & m_rawinst;
        const vector<group_entry>           & m_groups;
        const lbltbl_t                      & m_labels;
        const ScriptedSequence::consttbl_t  & m_constants;
        const ScriptedSequence::strtblset_t & m_strings;
        ScriptedSequence::grptbl_t          * m_poutgrp;
        size_t                                m_curgroup;           //Group we currently output into
        size_t                                m_curdataoffset;      //Offset in bytes within the Data chunk
        vector<size_t>                        m_grpends;            //The offset within the data chunk where each groups ends
        size_t                                m_datablocklen;       //Length of the data block in bytes
        OpCodeClassifier                      m_instfinder;
        size_t                                m_grptblandathdrlen;  //The offset the instructions begins at. Important for locating group starts, and jump labels
    };

//=======================================================================================
//  SSB Parser
//=======================================================================================
    /*
        SSB_Parser
            Parse SSB files.
    */
    template<typename _Init>
        class SSB_Parser
    {
    public:
        typedef _Init initer;

        SSB_Parser( _Init beg, _Init end, eOpCodeVersion scrver, eGameRegion scrloc )
            :m_beg(beg), m_end(end), m_cur(beg), m_scrversion(scrver), m_scrRegion(scrloc), m_lblcnt(0), 
             m_nbgroups(0), m_nbconsts(0), m_nbstrs(0),m_hdrlen(0),m_datablocklen(0), m_constlutbeg(0), m_stringlutbeg(0),
            m_datahdrgrouplen(0)
        {}

        ScriptedSequence Parse()
        {
            m_out = std::move(ScriptedSequence());
            m_lblcnt = 0;
            ParseHeader   ();
            ParseGroups   ();
            ParseConstants();
            ParseStrings  ();
            //PrepareGroups ();

            ParseCode     ();
            //ProcessCode   ();


            ScriptProcessor( m_rawinst, 
                             m_grps, 
                             m_metalabelpos, 
                             m_out.ConstTbl(), 
                             m_out.StrTblSet(), 
                             m_datablocklen, 
                             m_scrversion )(m_out);
            return std::move(m_out);
        }

    private:

        void ParseHeader()
        {
            uint16_t scriptdatalen = 0;
            uint16_t constdatalen  = 0;

            if( m_scrRegion == eGameRegion::NorthAmerica )
            {
                ssb_header hdr;
                m_hdrlen = ssb_header::LEN;
                m_cur = hdr.ReadFromContainer( m_cur, m_end );

                m_nbconsts     = hdr.nbconst;
                m_nbstrs       = hdr.nbstrs;
                scriptdatalen  = hdr.scriptdatlen;
                constdatalen   = hdr.consttbllen;
                m_stringblksSizes.push_back( hdr.strtbllen * ScriptWordLen );
            }
            else if( m_scrRegion == eGameRegion::Europe )
            {
                ssb_header_pal hdr;
                m_hdrlen = ssb_header_pal::LEN;
                m_cur = hdr.ReadFromContainer( m_cur, m_end );

                m_nbconsts    = hdr.nbconst;
                m_nbstrs      = hdr.nbstrs;
                scriptdatalen = hdr.scriptdatlen;
                constdatalen  = hdr.consttbllen;
                m_stringblksSizes.push_back( hdr.strenglen * ScriptWordLen );
                m_stringblksSizes.push_back( hdr.strfrelen * ScriptWordLen );
                m_stringblksSizes.push_back( hdr.strgerlen * ScriptWordLen );
                m_stringblksSizes.push_back( hdr.stritalen * ScriptWordLen );
                m_stringblksSizes.push_back( hdr.strspalen * ScriptWordLen );
            }
            else if( m_scrRegion == eGameRegion::Japan )
            {
                ssb_header hdr;
                m_hdrlen = ssb_header::LEN;
                m_cur = hdr.ReadFromContainer( m_cur, m_end );

                m_nbconsts     = hdr.nbconst;
                m_nbstrs       = hdr.nbstrs;
                scriptdatalen  = hdr.scriptdatlen;
                constdatalen   = hdr.consttbllen;
                m_stringblksSizes.push_back( hdr.strtbllen * ScriptWordLen );
            }
            else
            {
                cout<<"SSB_Parser::ParseHeader(): Unknown script region!!\n";
                assert(false);
            }

            //Parse Data header
            ssb_data_hdr dathdr;
            m_cur = dathdr.ReadFromContainer( m_cur, m_end );

            //Compute offsets and etc
            m_datablocklen      = (dathdr.datalen * ScriptWordLen);
            m_constlutbeg       = m_hdrlen + m_datablocklen;         //Group table is included into the datalen
            m_stringlutbeg      = m_hdrlen + (scriptdatalen * ScriptWordLen) + (constdatalen*2);
            m_nbgroups          = dathdr.nbgrps;
            m_datahdrgrouplen   = ssb_data_hdr::LEN + (m_nbgroups * group_entry::LEN);
        }

        inline void ParseGroups()
        {
            m_grps.resize(m_nbgroups);
            //Grab all groups
            for( size_t cntgrp = 0; cntgrp < m_nbgroups; ++cntgrp )
                m_cur = m_grps[cntgrp].ReadFromContainer( m_cur, m_end );
        }



        //void ParseCode()
        //{
        //    if( m_scrversion == eOpCodeVersion::EoS ) 
        //        ParseCodeWithOpCodeFinder(OpCodeFinderPicker<eOpCodeVersion::EoS>(), OpCodeNumberPicker<eOpCodeVersion::EoS>());
        //    else if( m_scrversion == eOpCodeVersion::EoTD )
        //        ParseCodeWithOpCodeFinder(OpCodeFinderPicker<eOpCodeVersion::EoTD>(), OpCodeNumberPicker<eOpCodeVersion::EoTD>() );
        //    else
        //    {
        //        clog << "\n<!>- SSB_Parser::ParseCode() : INVALID SCRIPT VERSION!!\n";
        //        assert(false);
        //    }
        //}

        //template<typename _InstFinder, typename _InstNumber>
        //    void ParseCodeWithOpCodeFinder(_InstFinder & opcodefinder, _InstNumber & opcodenumber)
        //{
        //    //Iterate through all group and grab their instructions.
        //    for( size_t cntgrp = 0; cntgrp < m_dathdr.nbgrps; ++cntgrp )
        //    {
        //        ScriptInstrGrp grp;
        //        size_t absgrpbeg = (m_grps[cntgrp].begoffset * ScriptWordLen) + m_hdrlen;
        //        size_t absgrpend = ((cntgrp +1) < m_dathdr.nbgrps)?
        //                            (m_grps[cntgrp+1].begoffset * ScriptWordLen) + m_hdrlen :   //If we have another group after, this is the end
        //                            m_datablocklen + m_hdrlen;                                       //If we have no group after, end of script data is end


        //        grp.instructions = std::move( ParseInstructionSequence( absgrpbeg, 
        //                                                                absgrpend,
        //                                                                opcodefinder,
        //                                                                opcodenumber ) );
        //        grp.type         = m_grps[cntgrp].type;
        //        grp.unk2         = m_grps[cntgrp].unk2;
        //        m_out.Groups().push_back( std::move(grp) );
        //    }
        //}

//
//        template<typename _InstFinder, typename _InstNumber>
//            deque<ScriptInstruction> ParseInstructionSequence( size_t foffset, size_t endoffset, _InstFinder & opcodefinder, _InstNumber & opcodenumber )
//        {
//            deque<ScriptInstruction> sequence;
//            m_cur = m_beg;
//            std::advance( m_cur, foffset ); 
//            auto itendseq= m_beg;
//            std::advance( itendseq, endoffset);
//
//
//            while( m_cur != itendseq )
//            {
//                uint16_t curop = utils::ReadIntFromBytes<uint16_t>( m_cur, itendseq );
//
//                if( curop < opcodenumber() )
//                {
//                    OpCodeInfoWrapper opcodedata = opcodefinder(curop); 
//                    
//                    if( !opcodedata )
//                    {
//#ifdef _DEBUG
//                        assert(false);
//#endif
//                        throw std::runtime_error("SSB_Parser::ParseInstructionSequence() : Unknown Opcode!");
//                    }
//                    ParseCommand( foffset, itendseq, curop, sequence, opcodedata );
//                }
//                else
//                {
//                    ParseData( foffset, curop, sequence );
//                }
//                foffset += ScriptWordLen;
//            }
//
//            return std::move(sequence);
//        }

        //void ParseCommand( size_t foffset, initer & itendseq, uint16_t curop, deque<ScriptInstruction> & out_queue, const OpCodeInfoWrapper & codeinfo  )
        //{
        //    ScriptInstruction inst;
        //    inst.type  = eInstructionType::Command;
        //    inst.value = curop;

        //    if( codeinfo.NbParams() >= 0 && m_cur != itendseq )
        //    {
        //        const uint16_t nbparams = codeinfo.NbParams();
        //        size_t cntparam = 0;
        //        for( ; cntparam < nbparams && m_cur != itendseq; ++cntparam )
        //        {
        //            inst.parameters.push_back( utils::ReadIntFromBytes<uint16_t>(m_cur, itendseq) );
        //        }
        //        foffset += cntparam * ScriptWordLen;

        //        if( cntparam != nbparams )
        //            clog << "\n<!>- Found instruction with not enough bytes left to assemble all its parameters at offset 0x" <<hex <<uppercase <<foffset <<dec <<nouppercase <<"\n";
        //        else
        //            out_queue.push_back(std::move(inst));
        //    }
        //    else
        //    {
        //        clog << "\n<!>- Found instruction with -1 parameter number in this script! Offset 0x" <<hex <<uppercase <<foffset <<dec <<nouppercase <<"\n";
        //        out_queue.push_back(std::move(inst));
        //    }
        //}


        void ParseCommand( size_t                                foffset, 
                           initer                              & itcur, 
                           initer                              & itendseq, 
                           uint16_t                              curop, 
                           const OpCodeInfoWrapper             & codeinfo  )
        {
            ScriptInstruction inst;
            inst.type  = eInstructionType::Command;
            inst.value = curop;

            if( codeinfo.NbParams() >= 0 && itcur != itendseq )
            {
                const uint16_t nbparams = codeinfo.NbParams();
                size_t cntparam = 0;
                for( ; cntparam < nbparams && itcur != itendseq; ++cntparam )
                {
                    HandleParameter(cntparam, inst, codeinfo, itcur, itendseq);
                }

                //!#TODO: It would be easier if we'd handle this error case in the calling function..
                if( cntparam != nbparams )
                    clog << "\n<!>- Found instruction with not enough bytes left to assemble all its parameters at offset 0x" <<hex <<uppercase <<foffset <<dec <<nouppercase <<"\n";
                else
                    m_rawinst.push_back(std::move(inst));

                foffset += cntparam * ScriptWordLen; //!#TODO: Maybe we shouldn't do this here..
            }
            else if( codeinfo.NbParams() == -1 && itcur != itendseq  )
            {
#if 1
                clog << "\n<!>- Found instruction with -1 parameter number in this script! Offset 0x" <<hex <<uppercase <<foffset <<dec <<nouppercase <<"\n";
                //!#TODO: -1 param instructions use the next 16bits word to indicate the amount of parameters to parse
                size_t cntparam = 0;
                size_t nbparams = PrepareParameterValue( utils::ReadIntFromBytes<int16_t>(itcur,itendseq) ); //iterator is incremented here

                for( ; cntparam < nbparams && itcur != itendseq; ++cntparam )
                    HandleParameter(cntparam, inst, codeinfo, itcur, itendseq);

                //!#TODO: It would be easier if we'd handle this error case in the calling function..
                if( cntparam != nbparams )
                    clog << "\n<!>- Found -1 instruction with not enough bytes left to assemble all its parameters at offset 0x" <<hex <<uppercase <<foffset <<dec <<nouppercase <<"\n";
                else
                    m_rawinst.push_back(std::move(inst));

                foffset += ScriptWordLen + (nbparams * ScriptWordLen);
#else
                m_rawinst.push_back(std::move(inst));
#endif
            }
        }

        inline void HandleParameter( size_t cntparam, ScriptInstruction & destinst, const OpCodeInfoWrapper & codeinfo, initer & itcur, initer & itendseq )
        {
            destinst.parameters.push_back( utils::ReadIntFromBytes<uint16_t>(itcur, itendseq) );

            if( cntparam < codeinfo.ParamInfo().size() )
                CheckAndMarkJumps(destinst.parameters.back(), codeinfo.ParamInfo()[cntparam].ptype );
        }

        //Also updates the value of the opcode if needed
        inline void CheckAndMarkJumps( uint16_t & pval, eOpParamTypes ptype )
        {
            if( ptype == eOpParamTypes::InstructionOffset || ptype == eOpParamTypes::CaseJumpOffset)
            {
                auto empres = m_metalabelpos.emplace( std::make_pair( PrepareParameterValue(pval) * ScriptWordLen, m_lblcnt ) );
                pval = m_lblcnt; //set the value to the label's value.

                if( empres.second )//Increment only if there was a new label added!
                    ++m_lblcnt;
            }
        }

        void ParseData( size_t foffset, uint16_t curop )
        {
            if( utils::LibWide().isLogOn() )
                clog<<"0x" <<hex <<uppercase <<foffset  <<" - Got data word 0x" <<curop <<" \n" <<nouppercase <<dec;

            //The instruction is actually a data word
            ScriptInstruction inst;
            inst.type  = eInstructionType::Data;
            inst.value = curop;
            m_rawinst.push_back(std::move(inst));
        }

        void ParseConstants()
        {
            if( !m_nbconsts )
                return;

            const size_t strlutlen = (m_nbstrs * 2); // In the file, the offset for each constants in the constant table includes the 
                                                     // length of the string lookup table(string pointers). Here, to compensate
                                                     // we subtract the length of the string LUT from each pointer read.
            m_out.ConstTbl() = std::move(ParseOffsetTblAndStrings<ScriptedSequence::consttbl_t>( m_constlutbeg, m_constlutbeg, m_nbconsts, strlutlen ));
        }

        void ParseStrings()
        {
            if( !m_nbstrs )
                return;

            //Parse the strings for any languages we have
            size_t strparseoffset = m_stringlutbeg;
            size_t begoffset      = ( m_nbconsts != 0 )? m_constlutbeg : m_stringlutbeg;

            for( size_t i = 0; i < m_stringblksSizes.size(); ++i )
            {
                m_out.InsertStrLanguage( static_cast<eGameLanguages>(i), std::move(ParseOffsetTblAndStrings<ScriptedSequence::strtbl_t>( strparseoffset, begoffset, m_nbstrs )) );
                strparseoffset += m_stringblksSizes[i]; //Add the size of the last block, so we have the offset of the next table
            }
        }

        /*
            relptroff == The position in the file against which the offsets in the table are added to.
            offsetdiff == this value will be subtracted from every ptr read in the table.
        */
        template<class _ContainerT>
            _ContainerT ParseOffsetTblAndStrings( size_t foffset, uint16_t relptroff, uint16_t nbtoparse, long offsetdiff=0 )
        {
            _ContainerT strings;

                //Parse regular strings here
                initer itoreltblbeg = m_beg;
                std::advance( itoreltblbeg, relptroff);
                initer itluttable = m_beg;
                std::advance(itluttable, foffset);
            
                assert( itoreltblbeg != m_end );

                //Parse string table
                for( size_t cntstr = 0; cntstr < nbtoparse && itluttable != m_end; ++cntstr )
                {
                    uint16_t stroffset = utils::ReadIntFromBytes<uint16_t>( itluttable, m_end ) - offsetdiff; //Offset is in bytes this time!
                    initer   itstr     = itoreltblbeg;
                    std::advance(itstr,stroffset);
                    strings.push_back( std::move(utils::ReadCStrFromBytes( itstr, m_end )) );
                }

            return std::move(strings);
        }

        void ParseCode()
        {
            if( m_scrversion == eOpCodeVersion::EoS ) 
                ParseCode(OpCodeFinderPicker<eOpCodeVersion::EoS>(), OpCodeNumberPicker<eOpCodeVersion::EoS>());
            else if( m_scrversion == eOpCodeVersion::EoTD )
                ParseCode(OpCodeFinderPicker<eOpCodeVersion::EoTD>(), OpCodeNumberPicker<eOpCodeVersion::EoTD>() );
            else
            {
                clog << "\n<!>- SSB_Parser::ParseCode() : INVALID SCRIPT VERSION!!\n";
                assert(false);
            }
        }

        template<typename _InstFinder, typename _InstNumber>
            void ParseCode(_InstFinder & opcodefinder, _InstNumber & opcodenumber)
        {
            //Iterate once through the entire code, regardless of groups, list all jump targets, and parse all operations
            const size_t instbeg   = m_hdrlen + ssb_data_hdr::LEN + (m_grps.size() * group_entry::LEN);
            const size_t instend   = m_hdrlen + m_datablocklen;
            initer       itcollect = m_beg;
            initer       itdatabeg = m_beg;
            initer       itdataend = m_beg;
            std::advance( itcollect, instbeg );
            std::advance( itdatabeg, m_hdrlen );
            std::advance( itdataend, instend );

            
            size_t instdataoffset = 0; //Offset relative to the beginning of the data

            while( itcollect != itdataend )
            {
                uint16_t curop = utils::ReadIntFromBytes<uint16_t>( itcollect, itdataend );

                if( curop < opcodenumber() )
                {
                    OpCodeInfoWrapper opcodedata = opcodefinder(curop); 
                    
                    if( !opcodedata )
                    {
#ifdef _DEBUG
                        assert(false);
#endif                  
                        stringstream sstr;
                        sstr <<"SSB_Parser::ParseInstructionSequence() : Unknown Opcode at absolute offset  " <<hex <<uppercase <<instdataoffset + instbeg <<"!";
                        throw std::runtime_error(sstr.str());
                    }
                    ParseCommand( instdataoffset, itcollect, itdataend, curop, opcodedata );
                }
                else
                {
                    ParseData( instdataoffset, curop );
                }
                instdataoffset += ScriptWordLen; //Count instructions and data. Parameters are added by when relevant!
            }
        }


    private:
        //Iterators
        initer              m_beg;
        initer              m_cur;
        initer              m_end;

        //TargetOutput
        ScriptedSequence    m_out;
        eOpCodeVersion      m_scrversion; 
        eGameRegion         m_scrRegion;

        //Offsets and lengths
        size_t              m_hdrlen;               //in bytes //Length of the SSB header for the current version + region
        size_t              m_datahdrgrouplen;      //in bytes //Length of the Data header and the group table
        size_t              m_datablocklen;         //in bytes //Length of the Data block in bytes
        size_t              m_constlutbeg;          //in bytes //Start of the lookup table for the constant strings ptrs
        size_t              m_stringlutbeg;         //in bytes //Start of strings lookup table for the strings
        vector<uint16_t>    m_stringblksSizes;      //in bytes //The lenghts of all strings blocks for each languages

        //Nb of entries
        uint16_t            m_nbstrs;
        uint16_t            m_nbconsts;
        uint16_t            m_nbgroups; 

        //Group data
        vector<group_entry>  m_grps;
        //vector<group_bounds> m_grpbounds;   

        //Label Assignement
        uint16_t             m_lblcnt;       //Used to assign label ids!
        lbltbl_t             m_metalabelpos; //first is position from beg of data, second is info on label

        //Instruction data buffer
        deque<ScriptInstruction> m_rawinst; //
    };


//=======================================================================================
//  SSB Writer
//=======================================================================================

    class SSBWriterTofile
    {
        typedef ostreambuf_iterator<char> outit_t;
    public:
        SSBWriterTofile(const pmd2::ScriptedSequence & scrdat, eGameRegion gloc, eOpCodeVersion opver)
            :m_scrdat(scrdat), m_scrRegion(gloc), m_opversion(opver)
        {
            if( m_scrRegion == eGameRegion::NorthAmerica || m_scrRegion == eGameRegion::NorthAmerica )
                m_stringblksSizes.resize(1,0);
            else if( m_scrRegion == eGameRegion::Europe )
                m_stringblksSizes.resize(5,0);
        }

        void Write(const std::string & scriptfile)
        {
            m_outf.open(scriptfile, ios::binary | ios::out);
            if( m_outf.bad() || !m_outf.is_open() )
                throw std::runtime_error("SSBWriterTofile::Write(): Couldn't open file " + scriptfile);

            m_hdrlen         = 0;
            m_datalen        = 0; 
            m_nbstrings      = 0;
            m_constoffset    = 0;
            m_constblksize   = 0;
            m_stringblockbeg = 0;

            if( m_scrRegion == eGameRegion::NorthAmerica || m_scrRegion == eGameRegion::Japan )
                m_hdrlen = ssb_header::LEN;
            else if( m_scrRegion == eGameRegion::Europe )
                m_hdrlen = ssb_header_pal::LEN;

            outit_t oit(m_outf);
            //#1 - Reserve data header 
            std::fill_n( oit, m_hdrlen + ssb_data_hdr::LEN, 0 );
            m_datalen += ssb_data_hdr::LEN; //Add to the total length immediately

            //#2 - Reserve group table
            std::fill_n( oit, m_scrdat.Groups().size() * group_entry::LEN, 0 );

            //#3 - Pre-Alloc/pre-calc stuff
            CalcAndVerifyNbStrings();
            m_grps.reserve( m_scrdat.Groups().size() );
            BuildLabelConversionTable();

            //#4 - Write code for each groups, constants, strings
            WriteCode(oit);
            WriteConstants(oit);
            WriteStrings(oit);

            //#5 - Header and group table written last, since the offsets and sizes are calculated as we go.
            m_outf.seekp(0, ios::beg);
            outit_t ithdr(m_outf);
            WriteHeader    (ithdr);
            WriteGroupTable(ithdr);
        }

    private:

        //Since we may have several string blocks to deal with, we want to make sure they're all the same size.
        void CalcAndVerifyNbStrings()
        {
            size_t siz = 0;
            for( auto & cur : m_scrdat.StrTblSet() )
            {
                if( siz == 0 )
                    siz = cur.second.size();
                else if( cur.second.size() != siz )
                    throw std::runtime_error("SSBWriterTofile::CalcAndVerifyNbStrings(): Size mismatch in one of the languages' string table!");
            }
            m_nbstrings = siz;
        }

        void BuildLabelConversionTable()
        {
            size_t curdataoffset = 0;

            for( const auto & grp : m_scrdat.Groups() )
            {
                for( const auto & inst : grp )
                {
                    if( inst.type == eInstructionType::Command || inst.type == eInstructionType::Data )
                        curdataoffset += ScriptWordLen + (ScriptWordLen * inst.parameters.size()); //Just count those
                    else if( inst.type == eInstructionType::MetaLabel )
                    {
                        m_labeltbl.emplace( std::make_pair( inst.value, curdataoffset / ScriptWordLen  ) );
                    }
                }
            }
        }

        void WriteHeader( outit_t & itw )
        {
#ifdef _DEBUG
                assert(m_stringblksSizes.size()>= 1);
#endif // _DEBUG
            if( m_scrRegion == eGameRegion::NorthAmerica )
            {
                ssb_header hdr;
                hdr.nbconst      = m_scrdat.ConstTbl().size();
                hdr.nbstrs       = m_nbstrings;
                hdr.scriptdatlen = TryConvertToScriptLen(m_datalen);
                hdr.consttbllen  = TryConvertToScriptLen(m_constblksize);
                hdr.strtbllen    = TryConvertToScriptLen(m_stringblksSizes.front());
                hdr.unk1         = 0; //Unk1 seems to be completely useless, so we're putting in random junk
                itw = hdr.WriteToContainer(itw);
            }
            else if( m_scrRegion == eGameRegion::Europe )
            {
#ifdef _DEBUG
                assert(m_stringblksSizes.size()== 5);
#endif // _DEBUG
                ssb_header_pal hdr;
                hdr.nbconst      = m_scrdat.ConstTbl().size();
                hdr.nbstrs       = m_nbstrings;
                hdr.scriptdatlen = TryConvertToScriptLen(m_datalen);
                hdr.consttbllen  = TryConvertToScriptLen(m_constblksize);
                if( m_nbstrings != 0 )
                {
                    hdr.strenglen = TryConvertToScriptLen(m_stringblksSizes[0]);
                    hdr.strfrelen = TryConvertToScriptLen(m_stringblksSizes[1]);
                    hdr.strgerlen = TryConvertToScriptLen(m_stringblksSizes[2]);
                    hdr.stritalen = TryConvertToScriptLen(m_stringblksSizes[3]);
                    hdr.strspalen = TryConvertToScriptLen(m_stringblksSizes[4]);
                }
                else
                {
                    hdr.strenglen = 0;
                    hdr.strfrelen = 0;
                    hdr.strgerlen = 0;
                    hdr.stritalen = 0;
                    hdr.strspalen = 0;
                }
                itw = hdr.WriteToContainer(itw);
            }
            else if( m_scrRegion == eGameRegion::Japan )
            {
                //The japanese game makes no distinction between strings and constants, and just places everything in the constant slot
                ssb_header hdr;
                hdr.nbconst      = m_scrdat.ConstTbl().size() + m_nbstrings;
                hdr.nbstrs       = 0;
                hdr.scriptdatlen = TryConvertToScriptLen(m_datalen);
                hdr.consttbllen  = TryConvertToScriptLen(m_constblksize);
                hdr.strtbllen    = 0;
                hdr.unk1         = 0; //Unk1 seems to be completely useless, so we're putting in random junk
                itw = hdr.WriteToContainer(itw);
            }

            ssb_data_hdr dathdr;
            dathdr.nbgrps  = m_scrdat.Groups().size();

            if( m_constoffset > 0 )
                dathdr.datalen = TryConvertToScriptLen(m_constoffset - m_hdrlen); //Const offset table isn't counted in this value, so we can't use m_datalen
            else 
                dathdr.datalen = TryConvertToScriptLen(m_datalen); //If no const table, we can set this to m_datalen
            itw = dathdr.WriteToContainer(itw);
        }

        //Write the table after the data header listing all the instruction groups
        void WriteGroupTable( outit_t & itw )
        {
#ifdef _DEBUG   //!#REMOVEME: For testing
            assert(!m_grps.empty()); //There is always at least one group!
#endif
            for( const auto & entry : m_grps )
            {
                itw = entry.WriteToContainer(itw);
            }
        }


        void WriteCode( outit_t & itw )
        {
            for( const auto & grp : m_scrdat.Groups() )
            {
                //Add a group entry for the current instruction group
                group_entry grent;
                grent.begoffset = TryConvertToScriptLen( (static_cast<uint16_t>(m_outf.tellp()) -  m_hdrlen) );
                grent.type      = grp.type;
                grent.unk2      = grp.unk2;
                m_grps.push_back(grent);
                m_datalen += group_entry::LEN;

                //Write the content of the group
                for( const auto & inst : grp )
                    WriteInstruction(itw,inst);
            }
        }

        void WriteInstruction( outit_t & itw, const ScriptInstruction & inst )
        {
            if( inst.type == eInstructionType::Command  )
            {
                itw = utils::WriteIntToBytes( inst.value, itw );
                m_datalen += ScriptWordLen;

                //!#TODO: We might want to add something here to handle references to file offsets used as parameters
                size_t cntparams = 0;
                for( const auto & param : inst.parameters )
                {
                    //!#TODO: If parameter is jump address, take the label id and use it on the label table

                    OpCodeInfoWrapper codeinf;
                    if( m_opversion == eOpCodeVersion::EoS )
                        codeinf = OpCodeFinderPicker<eOpCodeVersion::EoS>()(inst.value);
                    else if( m_opversion == eOpCodeVersion::EoTD )
                        codeinf = OpCodeFinderPicker<eOpCodeVersion::EoTD>()(inst.value);

                    if( codeinf && 
                        codeinf.ParamInfo().size() > cntparams && 
                       codeinf.ParamInfo()[cntparams].ptype == eOpParamTypes::InstructionOffset )
                    {
                        auto itf = m_labeltbl.find(param);
                        if( itf != m_labeltbl.end() )
                            itw = utils::WriteIntToBytes( itf->second, itw );
                        else
                        {
                            assert(false);
                        }
                    }
                    else
                    {
                        itw = utils::WriteIntToBytes( param, itw );
                    }

                    
                    m_datalen += ScriptWordLen;
                    ++cntparams;
                }
            }
            else if( inst.type == eInstructionType::Data )
            {
                itw = utils::WriteIntToBytes( inst.value, itw );
                m_datalen += ScriptWordLen;
            }
            else
            {
                //Handle META
            }
        }

        void CheckAndHandleJumpInstructions(outit_t & itw, const ScriptInstruction & inst)
        {
            m_labeltbl;
        }
        

        void WriteConstants( outit_t & itw )
        {
            if( m_scrdat.ConstTbl().empty() )
                return;
            //**The constant pointer table counts as part of the script data, but not the constant strings it points to for some weird reasons!!**
            //**Also, the offsets in the tables include the length of the string ptr table!**
            const streampos befconsttbl = m_outf.tellp();
            m_constoffset = static_cast<size_t>(befconsttbl);   //Save the location where we'll write the constant ptr table at, for the data header
            
            const uint16_t  sizcptrtbl     = m_scrdat.ConstTbl().size() * ScriptWordLen;
            const uint16_t  szstringptrtbl = m_nbstrings * ScriptWordLen;
            m_datalen += sizcptrtbl;    //Add the length of the table to the scriptdata length value for the header
            m_constblksize = WriteTableAndStrings( itw, m_scrdat.ConstTbl(),szstringptrtbl); //The constant strings data is not counted in datalen!




            //
            //size_t cntconst = 0;
            //const streampos befconsttbl = m_outf.tellp();
            //m_constoffset = static_cast<size_t>(befconsttbl);   //Save the location where we'll write the constant ptr table

            ////reserve table, so we can write the offsets as we go
            //const uint16_t  sizcptrtbl  = m_scrdat.ConstTbl().size() * ScriptWordLen;
            //std::fill_n( itw, sizcptrtbl, 0 );
            //m_datalen += sizcptrtbl;    //Add the length of the table to the scriptdata length value for the header

            ////Write constant strings
            //const streampos befconstdata = m_outf.tellp();
            //for( const auto & constant : m_scrdat.ConstTbl() )
            //{
            //    //Write offset in table 
            //    streampos curpos = m_outf.tellp();
            //    uint16_t curstroffset = (curpos - befconsttbl) / ScriptWordLen;

            //    m_outf.seekp( static_cast<size_t>(befconsttbl) + (cntconst * ScriptWordLen), ios::beg ); //Seek to the const ptr tbl
            //    *itw = curstroffset;            //Add offset to table
            //    m_outf.seekp( curpos, ios::beg ); //Seek back at the position we'll write the string at

            //    //write string
            //    //!#TODO: Convert escaped characters??
            //    itw = std::copy( constant.begin(), constant.end(), itw );
            //    *itw = '\0';
            //    ++itw;
            //    ++cntconst;
            //}

            ////Add some padding bytes if needed (padding is counted in the block's length)
            //utils::AppendPaddingBytes(itw, m_outf.tellp(), ScriptWordLen);

            ////Calculate the size of the constant strings data
            //m_constblksize = m_outf.tellp() - befconstdata;
        }

        /*
            WriteStrings
                Write the strings blocks
        */
        void WriteStrings( outit_t & itw )
        {
            if( m_scrdat.StrTblSet().empty() )
                return;

            size_t          cntstrblk       = 0;
            const streampos befstrptrs      = m_outf.tellp();
            const uint16_t  lengthconstdata = (m_scrdat.ConstTbl().size() * ScriptWordLen) + m_constblksize; //The length of the constant ptr tbl and the constant data!
            const uint16_t  szstringptrtbl = m_nbstrings * ScriptWordLen;
            m_stringblockbeg = static_cast<size_t>(befstrptrs); //Save the starting position of the string data, for later

            if( !m_scrdat.StrTblSet().empty() && m_scrdat.StrTblSet().size() != m_stringblksSizes.size() )
            {
#ifdef _DEBUG
                assert(false);
#endif
                throw std::runtime_error("SSBWriterToFile::WriteStrings(): Mismatch in expected script string blocks to ouput!!");
            }

            for( const auto & strblk : m_scrdat.StrTblSet() )
            {
                //Write each string blocks and save the length of the data into our table for later. 
                //**String block sizes include the ptr table!**
                m_stringblksSizes[cntstrblk] = WriteTableAndStrings( itw, strblk.second, lengthconstdata ) + szstringptrtbl; //We need to count the offset table too!!
                ++cntstrblk;
            }
        }


        /*
            WriteTableAndStrings
                Writes a string block, either the constants' strings or strings' strings
                Returns the length in bytes of the string data, **not counting the ptr table!**
        */
        template<class _CNT_T>
            size_t WriteTableAndStrings( outit_t      & itw,
                                         const _CNT_T & container,              //What contains the strings to write(std container needs begin() end() size() and const_iterator)
                                         size_t         ptrtbloffsebytes = 0 ) //Offset in **bytes** to add to all ptrs in the ptr table
        {
            size_t          cntstr     = 0;
            const streampos befptrs    = m_outf.tellp();
            const uint16_t  sizcptrtbl = (container.size() * ScriptWordLen);
            
            //Reserve pointer table so we can write there as we go
            std::fill_n( itw, sizcptrtbl, 0 );

            //Write strings
            const streampos befdata = m_outf.tellp();
            for( const auto & str : container )
            {
                //Write offset in table 
                streampos curpos = m_outf.tellp();

                m_outf.seekp( static_cast<size_t>(befptrs) + (cntstr * ScriptWordLen), ios::beg ); //Seek to the const ptr tbl
                itw = utils::WriteIntToBytes<uint16_t>( (ptrtbloffsebytes + (curpos - befptrs)), itw );            //Add offset to table
                m_outf.seekp( curpos, ios::beg ); //Seek back at the position we'll write the string at

                //write string
                //!#TODO: Convert escaped characters??
                itw = std::copy( str.begin(), str.end(), itw );
                *itw = '\0'; //Append zero
                ++itw;
                ++cntstr;
            }
            //Add some padding bytes if needed (padding is counted in the block's length)
            utils::AppendPaddingBytes(itw, m_outf.tellp(), ScriptWordLen);

            //Return the size of the constant strings data
            return m_outf.tellp() - befdata;
        }


        /*
            TryConvertToScriptLen
                This will divide the size/offset in bytes by 2, and validate if the result too big for the 16 bits of a word. 
                Throws an exception in that case! Otherwise, just returns the value divided by 2
        */
        inline uint16_t TryConvertToScriptLen( const streampos & lengthinbytes )
        {
            const uint32_t scrlen = lengthinbytes / ScriptWordLen;
            if( scrlen > std::numeric_limits<uint16_t>::max() )
                throw std::runtime_error("SSBWriterToFile::TryConvertToScriptLen(): Constant block size exceeds the length of a 16 bits word!!");
            return static_cast<uint16_t>(scrlen);
        }

    private:
        const pmd2::ScriptedSequence & m_scrdat;
        uint16_t            m_hdrlen; 

        size_t              m_nbstrings;
        vector<uint16_t>    m_stringblksSizes;     //in bytes //The lenghts of all strings blocks for each languages
        uint16_t            m_constblksize;        //in bytes //The length of the constant data block

        size_t              m_datalen;             //in bytes //Length of the Data block in bytes
        size_t              m_constoffset;         //in bytes //Start of the constant block from  start of file
        size_t              m_stringblockbeg;      //in bytes //Start of strings blocks from  start of file
        vector<group_entry> m_grps;

        unordered_map<uint16_t,uint16_t> m_labeltbl; //First is label ID, second is label offset in words

        eOpCodeVersion m_opversion; 
        eGameRegion    m_scrRegion;

        ofstream       m_outf;
    };

//=======================================================================================
//  Functions
//=======================================================================================
    /*
        ParseScript
    */
    pmd2::ScriptedSequence ParseScript(const std::string & scriptfile, eGameRegion gloc, eGameVersion gvers)
    {
        vector<uint8_t> fdata( std::move(utils::io::ReadFileToByteVector(scriptfile)) );
        eOpCodeVersion opvers = eOpCodeVersion::EoS;

        if( gvers == eGameVersion::EoS )
            opvers = eOpCodeVersion::EoS;
        else if( gvers == eGameVersion::EoT || gvers == eGameVersion::EoD )
            opvers = eOpCodeVersion::EoTD;
        else
            throw std::runtime_error("ParseScript(): Wrong game version!!");

        return std::move( SSB_Parser<vector<uint8_t>::const_iterator>(fdata.begin(), fdata.end(), opvers, gloc).Parse() );
    }

    /*
        WriteScript
    */
    void WriteScript( const std::string & scriptfile, const pmd2::ScriptedSequence & scrdat, eGameRegion gloc, eGameVersion gvers )
    {
        eOpCodeVersion opver =  (gvers == eGameVersion::EoS)?
                                    eOpCodeVersion::EoS :
                                (gvers == eGameVersion::EoD || gvers == eGameVersion::EoT)?
                                    eOpCodeVersion::EoTD :
                                    eOpCodeVersion::Invalid;
        SSBWriterTofile(scrdat, gloc, opver).Write(scriptfile);
    }


};
