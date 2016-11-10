#pragma once
#include "EagleLib/Nodes/Node.h"
#include "shared_ptr.hpp"
#include <MetaObject/Logging/Log.hpp>

#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/archives/xml.hpp>
#include <cereal/types/string.hpp>
namespace EagleLib
{
    EAGLE_EXPORTS bool Serialize(cereal::BinaryOutputArchive& ar, const EagleLib::Nodes::Node* obj);
    EAGLE_EXPORTS bool DeSerialize(cereal::BinaryInputArchive& ar, EagleLib::Nodes::Node* obj);
    EAGLE_EXPORTS bool Serialize(cereal::XMLOutputArchive& ar, const EagleLib::Nodes::Node* obj);
    EAGLE_EXPORTS bool DeSerialize(cereal::XMLInputArchive& ar, EagleLib::Nodes::Node* obj);
    EAGLE_EXPORTS bool Serialize(cereal::JSONOutputArchive& ar, const EagleLib::Nodes::Node* obj);
    EAGLE_EXPORTS bool DeSerialize(cereal::JSONInputArchive& ar, EagleLib::Nodes::Node* obj);
}
namespace cereal
{
    template<class AR, class T> void save(AR& ar, rcc::shared_ptr<T> const & m)
    {
        if (mo::CheckHasBeenSerialized(m->GetObjectId()))
        {
            std::string type = m->GetTypeName();
            ObjectId id = m->GetObjectId();
            ar(cereal::make_nvp("TypeId", id.m_ConstructorId));
            ar(cereal::make_nvp("InstanceId", id.m_PerTypeId));
            ar(make_nvp("TypeName", type));
            return;
        }
        if(std::is_base_of<EagleLib::Nodes::Node, T>::value)
        {
            EagleLib::Serialize(ar, m.Get());
        }else
        {
            mo::Serialize(ar, m.Get());
        }
        mo::SetHasBeenSerialized(m->GetObjectId());
    }

    template<class AR, class T> void load(AR& ar, rcc::shared_ptr<T> & m)
    {
        std::string type;
        ObjectId id;
        if (m.empty())
        {
            ar(make_nvp("TypeName", type));
            ar(cereal::make_nvp("TypeId", id.m_ConstructorId));
            ar(cereal::make_nvp("InstanceId", id.m_PerTypeId));
            if (auto obj = mo::MetaObjectFactory::Instance()->Get(id, type.c_str()))
            {
                m = obj;
            }
            else
            {
                m = mo::MetaObjectFactory::Instance()->Create(type.c_str());
            }
        }
        if (mo::CheckHasBeenSerialized(m->GetObjectId()))
            return;
        if (std::is_base_of<EagleLib::Nodes::Node, T>::value)
        {
            EagleLib::DeSerialize(ar, m.Get());
        }else
        {
            mo::DeSerialize(ar, m.Get());
        }
        mo::SetHasBeenSerialized(m->GetObjectId());
    }
    template<class AR, class T> void save(AR& ar, rcc::weak_ptr<T> const & m)
    {
        std::string type = m->GetTypeName();
        ObjectId id = m->GetObjectId();
        ar(cereal::make_nvp("TypeId", id.m_ConstructorId));
        ar(cereal::make_nvp("InstanceId", id.m_PerTypeId));
        ar(make_nvp("TypeName", type));
    }

    template<class AR, class T> void load(AR& ar, rcc::weak_ptr<T> & m)
    {
        std::string type;
        ObjectId id;
        ar(cereal::make_nvp("TypeId", id.m_ConstructorId));
        ar(cereal::make_nvp("InstanceId", id.m_PerTypeId));
        ar(make_nvp("TypeName", type));
        m.reset(mo::MetaObjectFactory::Instance()->Get(id, type.c_str()));
    }
}