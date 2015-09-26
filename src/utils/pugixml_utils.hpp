#ifndef PUGIXML_UTILS_HPP
#define PUGIXML_UTILS_HPP
/*
pugixml_utils.hpp
2015/03/26
psycommando@gmail.com
Description:
    A bunch of tools for doing repetitive things with the pugixml lib!
    Don't include in another header!!
*/
#include <string>
#include <pugixml.hpp>
#include <codecvt>
#include <locale>

namespace pugixmlutils
{

    /***************************************************************************************
        WriteCommentNode
            Helper for writing comments from std::string. Also helps with clarity.
    /***************************************************************************************/
    inline void WriteCommentNode( pugi::xml_node & node, const pugi::string_t & str )
    {
        using namespace pugi;
        node.append_child( xml_node_type::node_comment ).set_value( str.c_str() );
    }

    inline void WriteCommentNode( pugi::xml_node & node, const pugi::char_t * str )
    {
        using namespace pugi;
        node.append_child( xml_node_type::node_comment ).set_value(str);
    }

    /***************************************************************************************
        WriteNodeWithValue
            Helper for converting values to text, and putting it in a new node. 
            Also helps with clarity.
    /***************************************************************************************/
    template<class T>
        inline void WriteNodeWithValue( pugi::xml_node & parentnode, const pugi::string_t & name, T value )
    {
        using namespace pugi;
        using namespace std;
        parentnode.append_child(name.c_str()).append_child(node_pcdata).set_value( to_string(value).c_str() );
    }

    template<>
        inline void WriteNodeWithValue<const pugi::char_t *>( pugi::xml_node & parentnode, const pugi::string_t & name, const pugi::char_t * value )
    {
        using namespace pugi;
        parentnode.append_child(name.c_str()).append_child(node_pcdata).set_value(value);
    }

    template<>
        inline void WriteNodeWithValue<const pugi::string_t &>( pugi::xml_node & parentnode, const pugi::string_t & name, const pugi::string_t & value )
    {
        using namespace pugi;
        parentnode.append_child(name.c_str()).append_child(node_pcdata).set_value(value.c_str());
    }

    template<>
        inline void WriteNodeWithValue<pugi::string_t&&>( pugi::xml_node & parentnode, const pugi::string_t & name, pugi::string_t &&value )
    {
        using namespace pugi;
        parentnode.append_child(name.c_str()).append_child(node_pcdata).set_value(value.c_str());
    }

    template<>
        inline void WriteNodeWithValue<pugi::string_t>( pugi::xml_node & parentnode, const pugi::string_t & name, pugi::string_t value )
    {
        using namespace pugi;
        parentnode.append_child(name.c_str()).append_child(node_pcdata).set_value(value.c_str());
    }

    /***************************************************************************************
        AppendChildNode
            Makes appending a child node and setting its name easier, clearer with std::string.
    /***************************************************************************************/
    inline pugi::xml_node AppendChildNode( pugi::xml_node & parent, const pugi::string_t & childname )
    {
        return parent.append_child( childname.c_str() );
    }

    /***************************************************************************************
        AppendAttribute
            Makes appending an attribute and setting its value easier, clearer with std::string.
    ***************************************************************************************/
    template<class T>
        inline pugi::xml_attribute AppendAttribute( pugi::xml_node & parent, const pugi::string_t & name, T value ) 
    {
        using namespace pugi;
        xml_attribute att = parent.append_attribute(name.c_str());
#ifdef PUGIXML_WCHAR_MODE
        if( !att.set_value(std::to_wstring(value).c_str()) )
            throw std::runtime_error("pugixml couldn't set the value of attribute " + as_utf8(name) );
#else
        if( !att.set_value(std::to_string(value).c_str()) )
            throw std::runtime_error("pugixml couldn't set the value of attribute " + name);
#endif
        return std::move(att);
    }

    template<>
        inline pugi::xml_attribute AppendAttribute<const pugi::char_t*>( pugi::xml_node & parent, const pugi::string_t & name, const pugi::char_t* value )  
    {
        using namespace pugi;
        xml_attribute att = parent.append_attribute(name.c_str());

#ifdef PUGIXML_WCHAR_MODE
        if( !att.set_value(value) )
            throw std::runtime_error("pugixml couldn't set the value of attribute " + as_utf8(name) );
#else
        if( !att.set_value(value) )
            throw std::runtime_error("pugixml couldn't set the value of attribute " + name );
#endif
        return std::move(att);
    }

    template<>
        inline pugi::xml_attribute AppendAttribute<pugi::string_t>( pugi::xml_node & parent, const pugi::string_t & name, pugi::string_t value )  
    {
        using namespace pugi;
        xml_attribute att = parent.append_attribute(name.c_str());

#ifdef PUGIXML_WCHAR_MODE
        if( !att.set_value(value.c_str()) )
        {
            //std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
            throw std::runtime_error("pugixml couldn't set the value of attribute " + /*myconv.to_bytes(*/as_utf8(name)/*)*/ );
        }
#else
        if( !att.set_value(value.c_str()) )
            throw std::runtime_error("pugixml couldn't set the value of attribute " + name );
#endif
        return std::move(att);
    }
};

#endif