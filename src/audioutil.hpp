#ifndef AUDIO_UTIL_HPP
#define AUDIO_UTIL_HPP
/*
audioutil.hpp
2015/05/20
psycommando@gmail.com
Description: Code for the audioutil utility for Pokemon Mystery Dungeon : Explorers of Time/Darkness/Sky.
*/
#include <ppmdu/utils/cmdline_util.hpp>
#include <string>
#include <vector>

namespace audioutil
{
    class CAudioUtil : public utils::cmdl::CommandLineUtility
    {
    public:
        static CAudioUtil & GetInstance();

        // -- Overrides --
        //Those return their implementation specific arguments, options, and extra parameter lists.
        const std::vector<utils::cmdl::argumentparsing_t> & getArgumentsList()const;
        const std::vector<utils::cmdl::optionparsing_t>   & getOptionsList()const;
        const utils::cmdl::argumentparsing_t              * getExtraArg()const; //Returns nullptr if there is no extra arg. Extra args are args preceeded by a "+" character, usually used for handling files in batch !
       
        //For writing the title and readme!
        const std::string & getTitle()const;            //Name/Title of the program to put in the title!
        const std::string & getExeName()const;          //Name of the executable file!
        const std::string & getVersionString()const;    //Version number
        const std::string & getShortDescription()const; //Short description of what the program does for the header+title
        const std::string & getLongDescription()const;  //Long description of how the program works
        const std::string & getMiscSectionText()const;  //Text for copyrights, credits, thanks, misc..

        //Main method
        int Main(int argc, const char * argv[]);

    private:
        CAudioUtil();

        //Parse Arguments
        bool ParseInputPath  ( const std::string              & path );
        bool ParseOutputPath ( const std::string              & path );

        //Parsing Options


        //Execution
        void DetermineOperation();
        int  Execute           ();
        int  GatherArgs        ( int argc, const char * argv[] );

        //Exec methods

        //Constants
        static const std::string                                 Exe_Name;
        static const std::string                                 Title;
        static const std::string                                 Version;
        static const std::string                                 Short_Description;
        static const std::string                                 Long_Description;
        static const std::string                                 Misc_Text;
        static const std::vector<utils::cmdl::argumentparsing_t> Arguments_List;
        static const std::vector<utils::cmdl::optionparsing_t>   Options_List;

        enum struct eOpMode
        {
            Invalid,

            ExportSWDLBank, //Export the main bank, and takes the presets of all the swd files accompanying each smd files in the same folder to build a soundfont!
            ExportSWDL,     //Export a SWDL file to a folder. The folder contains the wav, and anything else is turned into XML
            ExportSMDL,     //Export a SMDL file as a midi
            ExportSEDL,     //Export the SEDL as a midi and some XML

            BuildSWDL,      //Build a SWDL from a folder. Must contain XML info file. If no preset data present builds a simple wav bank.(samples are imported in the slot corresponding to their file name)
            BuildSMDL,      //Build a SMDL from a midi file and a similarly named XML file. XML file used to remap instruments from GM to the game's format, and to set the 2 unknown variables.
            BuildSEDL,      //Build SEDL from a folder a midi file and XML.
        };

        //Default filenames names

        //Variables
        std::string m_inputPath;      //This is the input path that was parsed 
        std::string m_outputPath;     //This is the output path that was parsed
        eOpMode     m_operationMode;  //This holds what the program should do

    };
};

#endif 