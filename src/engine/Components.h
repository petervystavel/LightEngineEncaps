#pragma once
#include "../core/framework.h"
#include "GCColor.h"
#include "Map.h"

// TODO Adding lots of stuff to the components
// TODO Transforms for colliders
// TODO Make sure IDs are handled differently

class GCGameObject;



enum FLAGS
{
	UPDATE          = 1 << 0,
	FIXED_UPDATE    = 1 << 1,
    RENDER          = 1 << 2,
};
inline FLAGS operator|( FLAGS a, FLAGS b ) { return static_cast<FLAGS>(static_cast<int>(a) | static_cast<int>(b)); }



class Component
{
friend class GCGameObject;
friend class GCUpdateManager;
friend class GCPhysicManager;
friend class GCRenderManager;
public:
    virtual const int GetID() = 0;
    
    void SetActive( bool active ) { m_active = active; }
    bool IsActive() { return m_active; }

    GCGameObject* GetGameObject() { return m_pGameObject; }

protected:
    Component( GCGameObject* pGameObject );
    virtual ~Component() = default;
    
    virtual void Start() {}
    virtual void Update() {}
    virtual void FixedUpdate() {}
    virtual void Render() {}
    virtual void Destroy() {}

    bool IsFlagSet( FLAGS flag ) { return ( m_flags & flag ) != 0; }

protected:
    inline static int componentCount = 0;
	inline static int m_flags = 0;
    GCGameObject* m_pGameObject;
    bool m_active;
    
    GCListNode<Component*>* m_pUpdateNode;
    GCListNode<Component*>* m_pPhysicsNode;
    GCListNode<Component*>* m_pRenderNode;

};



class SpriteRenderer : public Component
{
friend class GCGameObject;
friend class GCUpdateManager;
friend class GCPhysicManager;
friend class GCRenderManager;
public:
    static const int GetIDStatic() { return m_ID; }
    const int GetID() override { return m_ID; }
    
    void SetSprite() {};
    void SetColor( GCColor& color ) { m_color = color; }
    
    void GetSprite() {};
    GCColor& GetColor() { return m_color; }

protected:
	SpriteRenderer( GCGameObject* pGameObject ) : Component( pGameObject ) {};
    ~SpriteRenderer() override {}

    void Render() override {}
    void Destroy() override {}

protected:
    inline static const int m_ID = ++Component::componentCount;
    inline static int m_flags = RENDER;
    GCColor m_color;

};



class Collider : public Component
{
friend class GCGameObject;
friend class GCUpdateManager;
friend class GCPhysicManager;
friend class GCRenderManager;

public:
    Collider( GCGameObject* pGameObject );
    ~Collider() override {}

    void SetTrigger( bool trigger ) { m_trigger = trigger; }
    void SetVisible( bool showCollider ) { m_visible = showCollider; }
    
    bool IsTrigger() { return m_trigger; }
    bool IsVisible() { return m_visible; }

protected:
    inline static int m_flags = FIXED_UPDATE | RENDER;
    bool m_trigger;
    bool m_visible;

};



class BoxCollider : public Collider
{
friend class GCGameObject;
friend class GCUpdateManager;
friend class GCPhysicManager;
friend class GCRenderManager;
public:
    static const int GetIDStatic() { return m_ID; }
    const int GetID() override { return m_ID; }

    GCVEC2 GetSize() { return m_size; }
    void SetSize( GCVEC2 size ) { m_size = size; }

protected:
    BoxCollider( GCGameObject* pGameObject ) : Collider( pGameObject ) {};
    ~BoxCollider() override {}

    void FixedUpdate() override {}
    void Render() override {}
    void Destroy() override {}

protected:
    inline static const int m_ID = ++Component::componentCount;
    GCVEC2 m_size;

};



class CircleCollider : public Collider
{
friend class GCGameObject;
friend class GCUpdateManager;
friend class GCPhysicManager;
friend class GCRenderManager;
public:
    static const int GetIDStatic() { return m_ID; }
    const int GetID() override { return m_ID; }
    
    float GetRadius() { return m_radius; }
    void SetRadius( float radius ) { m_radius = radius; }

protected:
    CircleCollider( GCGameObject* pGameObject ) : Collider( pGameObject ) {};
    ~CircleCollider() override {}
    
    void FixedUpdate() override {}
    void Render() override {}
    void Destroy() override {}

protected:
    inline static const int m_ID = ++Component::componentCount;
    float m_radius;

};



class RigidBody : public Component
{
friend class GCGameObject;
friend class GCUpdateManager;
friend class GCPhysicManager;
friend class GCRenderManager;
public:
    static const int GetIDStatic() { return m_ID; }
    const int GetID() override { return m_ID; }
    
    void AddForce( GCVEC2 force ) {}

protected:
    RigidBody( GCGameObject* pGameObject );
    ~RigidBody() override {}
    
    void FixedUpdate() override;
    void Destroy() override {}

protected:
    inline static const int m_ID = ++Component::componentCount;
    inline static int m_flags = FIXED_UPDATE;
    GCVEC3 m_velocity;

};



class Animator : public Component
{
friend class GCGameObject;
friend class GCUpdateManager;
friend class GCPhysicManager;
friend class GCRenderManager;
public:
    static const int GetIDStatic() { return m_ID; }
    const int GetID() override { return m_ID; }

protected:
	Animator( GCGameObject* pGameObject ) : Component( pGameObject ) {};
    ~Animator() override {}
    
    void Update() override {}
    void Destroy() override {}

protected:
    inline static const int m_ID = ++Component::componentCount;
    inline static int m_flags = UPDATE;

};



class SoundMixer : public Component
{
friend class GCGameObject;
friend class GCUpdateManager;
friend class GCPhysicManager;
friend class GCRenderManager;
public:
    static const int GetIDStatic() { return m_ID; }
    const int GetID() override { return m_ID; }

protected:
	SoundMixer( GCGameObject* pGameObject ) : Component( pGameObject ) {};
    ~SoundMixer() override {}
    
    void Update() override {}
    void Destroy() override {}

protected:
    inline static const int m_ID = ++Component::componentCount;
    inline static int m_flags = UPDATE;

};



class Script : public Component
{
friend class GCGameObject;
friend class GCUpdateManager;
friend class GCPhysicManager;
friend class GCRenderManager;

protected:
    Script( GCGameObject* pGameObject ) : Component( pGameObject ) {};
    virtual ~Script() = default;
    
    virtual void OnTriggerEnter( Collider* collider ) {};
    virtual void OnTriggerStay( Collider* collider ) {};
    virtual void OnTriggerExit( Collider* collider ) {};

protected:
    inline static int scriptCount = (1<<15)-1;

};

#define CREATE_SCRIPT_START2( CLASS_NAME, INHERITANCE ) \
    class Script##CLASS_NAME : public INHERITANCE \
    { \
    public: \
        static const int GetIDStatic() { return m_ID; } \
        const int GetID() override { return m_ID; } \
     \
    protected: \
        Script##CLASS_NAME() = default; \
        ~Script##CLASS_NAME() = default; \
         \
        /*void Start() override; \
        void Update() override; \
        void FixedUpdate() override; \
        void Destroy() override; \
         \
        void OnTriggerEnter( Collider* collider ) override; \
        void OnTriggerStay( Collider* collider ) override; \
        void OnTriggerExit( Collider* collider ) override;*/ \
     \
    protected: \
        inline static const int m_ID = ++Script::scriptCount; \
        inline static int m_flags = UPDATE | FIXED_UPDATE; \
     \
    private:

#define CREATE_SCRIPT_START1( CLASS_NAME ) CREATE_SCRIPT_START2( CLASS_NAME, Script )

#define CREATE_SCRIPT_END };



// ChatGPT Code

// Helper macros to count arguments
#define COUNT_ARGS_IMPL2(_1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...) N
#define COUNT_ARGS_IMPL(args) COUNT_ARGS_IMPL2 args
#define COUNT_ARGS(...) COUNT_ARGS_IMPL((__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))

// Helper macros to select the correct macro based on argument count
#define CONCATE(a, b) a##b
#define MACRO_OVERLOAD(name, count) CONCATE(name, count)

// Main macro to select the correct PRINT macro based on the number of arguments
#define CREATE_SCRIPT_START(...) MACRO_OVERLOAD(CREATE_SCRIPT_START, COUNT_ARGS(__VA_ARGS__))(__VA_ARGS__)



