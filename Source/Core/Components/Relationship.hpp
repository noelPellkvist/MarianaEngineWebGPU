#pragma once
#include <entt/entt.hpp>

struct Relationship
{
    std::size_t children{0};
    entt::entity first{entt::null};
    entt::entity prev{entt::null};
    entt::entity next{entt::null};
    entt::entity parent{entt::null};

    void SetParent(entt::entity newParent, entt::entity newEntity, entt::registry& reg) {
        assert(parent == entt::null);
        parent = newParent;
        if (parent == entt::null) return;

        auto& p = reg.get<Relationship>(parent);
        p.children++;

        entt::entity lastChild = p.first;
        if(lastChild == entt::null)
        {
            p.first = newEntity;
            return;
        }
        while (reg.get<Relationship>(lastChild).next != entt::null) {
            lastChild = reg.get<Relationship>(lastChild).next;
        }

        auto& lastRelation = reg.get<Relationship>(lastChild);
        lastRelation.next = newEntity;
        auto& newRelation = reg.get<Relationship>(newEntity);
        newRelation.prev = lastChild; // Set the previous sibling to the last child
    }

    void AddChild(entt::entity newParent, entt::entity newChild, entt::registry& reg) {
        auto& c = reg.get<Relationship>(newChild);
        c.parent = newParent;
        children++;
        if (first == entt::null)
        {
            first = newChild;
            return;
        }
        entt::entity lastChild = first;
        while (reg.get<Relationship>(lastChild).next != entt::null) {
            lastChild = reg.get<Relationship>(lastChild).next;
        }
        auto& lastRelation = reg.get<Relationship>(lastChild);
        lastRelation.next = newChild;
        c.prev = lastChild;
    }
};