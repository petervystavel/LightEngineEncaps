#pragma once
#include "PhysicManager.h"
#include "UpdateManager.h"
#include "EventManager.h"
#include "SceneManager.h"

template <typename T>
class GCListNode;

class GCGameManager 
{
friend class GC;

protected:
    GCGameManager() = default;
    virtual ~GCGameManager() = default;
    
public: void Init();
public: void Update();
    void Init();
    void Update();
    
    void SetActiveGameManager();
    
protected:
    GCListNode<GCGameManager*>* m_pNode;
    GCPhysicManager m_pPhysicManager;
    GCUpdateManager m_pUpdateManager;
    GCEventManager m_pEventManager;
    GCSceneManager m_pSceneManager;
};
