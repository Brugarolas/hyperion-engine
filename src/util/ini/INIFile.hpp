/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_INI_FILE_HPP
#define HYPERION_INI_FILE_HPP

#include <core/containers/HashMap.hpp>

#include <asset/BufferedByteReader.hpp>

#include <Config.hpp>

namespace hyperion {

class INIFile
{
public:
    struct Element
    {
        static const Element empty;

        String          name;
        String          value;
        Array<String>   sub_elements;
    };

    struct Value
    {
        Array<Element> elements;

        const Element &GetValue() const
        {
            return elements.Any()
                ? elements.Front()
                : Element::empty;
        }

        const Element &GetValue(SizeType index) const
        {
            return index < elements.Size()
                ? elements[index]
                : Element::empty;
        }
    };

    using Section = HashMap<String, Value>;

    INIFile(const FilePath &path);
    ~INIFile() = default;

    bool IsValid() const
        { return m_is_valid; }

    const FilePath &GetFilePath() const
        { return m_path; }

    const HashMap<String, Section> &GetSections() const
        { return m_sections; }

    bool HasSection(const String &key) const
        { return m_sections.Contains(key); }

    Section &GetSection(const String &key)
        { return m_sections[key]; }

private:
    void Parse();

    bool                        m_is_valid;
    FilePath                    m_path;

    HashMap<String, Section>    m_sections;
};

} // namespace hyperion

#endif