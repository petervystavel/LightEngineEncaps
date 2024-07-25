#pragma once
#include "List.h"

class GCGameManager;
class Window;
class GCInputSystem;
class GCPhysicManager;
class GCUpdateManager;
class GCEventManager;
class GCSceneManager;
class GCScene;
class GCRenderManager;
struct HINSTANCE__;
typedef struct HINSTANCE__ *HINSTANCE;

class GC
{
    friend class GCGameManager;

private:
    GC() = delete;
    ~GC() = delete;

public:
    static GCGameManager* CreateGameManager( HINSTANCE hInstance );

    static GCGameManager* GetActiveGameManager();
    static Window* GetWindow();
    static GCInputSystem* GetActiveInputSystem();
    static GCEventManager* GetActiveEventManager();
    static GCPhysicManager* GetActivePhysicManager();
    static GCUpdateManager* GetActiveUpdateManager();
    static GCSceneManager* GetActiveSceneManager();
    static GCScene* GetActiveScene();
    static GCRenderManager* GetActiveRenderManager();

private:
    inline static GCList<GCGameManager*> m_pGameManagersList = GCList<GCGameManager*>();
    inline static GCGameManager* m_pActiveGameManager = nullptr;

};