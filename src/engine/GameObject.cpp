#include "pch.h"
#include "GameObject.h"

#include "Components.h"
#include "Scene.h"
#include "SceneManager.h"

// todo Children inheriting from the parent m_active
// todo GameObject being able to have multiple tags



//////////////////////////////////////////////////////////////////////////////
/// @brief Default constructor for the GCGameObject class.
/// 
/// @param pScene A pointer to the Scene the GameObject is in.
/// 
/// @note The GameObject won't be fully created until the "Creation Queue".
//////////////////////////////////////////////////////////////////////////////
GCGameObject::GCGameObject( GCScene* pScene )
{
    m_ID = s_gameObjectsCount++;
    m_pSceneNode = nullptr;
    m_pChildNode = nullptr;
    
    m_pScene = pScene;
    m_pParent = nullptr;
    
    m_created = false;
    m_active = true;
    m_name = "GameObject";
    m_tag = "";
    m_layer = 0;
}

////////////////////////////////////////////////////////////////////////////
/// @brief Updates the GameObject and its Components.
/// 
/// @note The GameObject won't update if not active or not fully created.
////////////////////////////////////////////////////////////////////////////
void GCGameObject::Update()
{
    if ( m_active == false || m_created == false ) return;
    Component* component; 
    for (int i = 1 ; i <= 7 ; i++)
        if ( m_componentsList.Find(i, component) == true ) 
            if ( component->m_active == true )
                component->Update();
}

////////////////////////////////////////////////////////////////////////////
/// @brief Renders the GameObject's Components related to rendering.
/// 
/// @note The GameObject won't render if not active or not fully created.
////////////////////////////////////////////////////////////////////////////
void GCGameObject::Render()
{
    if ( m_active == false || m_created == false ) return;
    // todo GameObject Render
}

///////////////////////////////////////////////////////////
/// @brief Adds the GameObject to the "Deletion Queue".
/// 
/// @note The GameObject will be deleted the next frame.
///////////////////////////////////////////////////////////
void GCGameObject::Destroy()
{
    GCSceneManager::AddGameObjectToDeleteQueue( this );
}



/////////////////////////////////////////////////////////////
/// @brief Sets a new parent to this GameObject.
/// 
/// @param pParent A pointer to the new parent GameObject.
/////////////////////////////////////////////////////////////
void GCGameObject::SetParent( GCGameObject* pParent )
{
    pParent->AddChild( this );
}

/////////////////////////////////////////////////////////
/// @brief Removes itself from it's parent GameObject.
/////////////////////////////////////////////////////////
void GCGameObject::RemoveParent()
{
    if ( m_pParent == nullptr ) return;
    m_pParent->RemoveChild( this );
}

/////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a new GameObject and adds it as a child to this GameObject.
/// 
/// @return A pointer to the newly created GameObject.
/////////////////////////////////////////////////////////////////////////////////
GCGameObject* GCGameObject::CreateChild()
{
    GCGameObject* pChild = m_pScene->CreateGameObject();
    AddChild( pChild ); //! Not really efficient ( verifications could be skipped as the GameObject was just created )
    return pChild;
}

///////////////////////////////////////////////////////////////////
/// @brief Adds a child to this GameObject.
/// 
/// @param pChild A pointer to the child GameObject to be added.
/// 
/// @warning You can't add yourself as your child.
/// @warning You can't add the same child twice.
/// @warning You can't add one of your ancestors as your child.
/// 
/// @note It will remove the child previous parent.
///////////////////////////////////////////////////////////////////
void GCGameObject::AddChild( GCGameObject* pChild )
{
    if ( pChild == this ) return;
    if ( pChild->m_pParent == this ) return;
    for ( GCGameObject* pAncestor = pChild->m_pParent; pAncestor != nullptr; pAncestor = pAncestor->m_pParent )
        if ( pChild == pAncestor ) return;
    
    pChild->RemoveParent();
    m_childrenList.PushBack( pChild );
    pChild->m_pChildNode = m_childrenList.GetLastNode();
    pChild->m_pParent = this;
    // todo Updating the transform so that the GameObject stays where it was before it was added
    // todo Assert for the errors instead of simply returning nothing
}

///////////////////////////////////////////////////////////////////////////////////////////
/// @brief Removes a child from this GameObject.
/// 
/// @param pChild A pointer to the child GameObject to be removed.
/// 
/// @warning You can't remove a GameObject that's not a direct child of this GameObject.
///////////////////////////////////////////////////////////////////////////////////////////
void GCGameObject::RemoveChild( GCGameObject* pChild )
{
    if ( pChild->m_pParent != this ) return;
    
    m_childrenList.RemoveNode( pChild->m_pChildNode );
    pChild->m_pChildNode = nullptr;
    pChild->m_pParent = nullptr;
    // todo Updating the transform so that the GameObject stays where it was before it was removed
    // todo Assert for the errors instead of simply returning nothing
}

/////////////////////////////////////////////////////
/// @brief Destroys every child of the GameObject.
/// 
/// @note Does not destroy the parent GameObject.
/////////////////////////////////////////////////////
void GCGameObject::DeleteChildren()
{
    for ( GCListNode<GCGameObject*>* pChildNode = m_childrenList.GetFirstNode(); pChildNode != nullptr; pChildNode = pChildNode->GetNext() )
        pChildNode->GetData()->Destroy();
    m_childrenList.Clear();
}



///////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Sets the active state of this GameObject.
/// 
/// @param active A boolean value indicating whether the GameObject should be active or not.
/// 
/// @note When a GameObject is not active, it will not be updated or rendered.
///////////////////////////////////////////////////////////////////////////////////////////////
void GCGameObject::SetActive( bool active ) { m_active = active; }

////////////////////////////////////////////////////////////////////////////////
/// @brief Sets the name of this GameObject.
/// 
/// @param name A string value indicating the new name of the GameObject.
/// 
/// @note The name doesn't have to be unique within the Scene.
/// @note The name can be used for identification purposes (not recommanded).
////////////////////////////////////////////////////////////////////////////////
void GCGameObject::SetName( const char* name ) { m_name = name; }

//////////////////////////////////////////////////////////////////////////
/// @brief Sets the tag of this GameObject.
/// 
/// @param tag A string value indicating the new tag of the GameObject.
/// 
/// @note The tag can be used for identification purposes.
//////////////////////////////////////////////////////////////////////////
void GCGameObject::SetTag( const char* tag ) { m_tag = tag; }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Sets the layer of this GameObject.
/// 
/// @param layer An integer indicating the new layer of the GameObject.
/// 
/// @note The layer determines the order in which GameObjects are rendered (higher layers are renderer on top).
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GCGameObject::SetLayer( int layer ) { m_layer = layer; }



//////////////////////////////////////////////////////////////////////
/// @return An integer indicating the unique ID of this GameObject.
//////////////////////////////////////////////////////////////////////
unsigned int GCGameObject::GetID() const { return m_ID; }

///////////////////////////////////////////////////////////
/// @return A pointer to the Scene the GameObject is in.
///////////////////////////////////////////////////////////
GCScene* GCGameObject::GetScene() const { return m_pScene; }

////////////////////////////////////////////////////
/// @return A pointer to the GameObject's parent.
////////////////////////////////////////////////////
GCGameObject* GCGameObject::GetParent() const { return m_pParent; }

//////////////////////////////////////////////////////////
/// @return A Linked List of the GameObject's children.
//////////////////////////////////////////////////////////
GCList<GCGameObject*> GCGameObject::GetChildren() const { return m_childrenList; }

/////////////////////////////////////////////////////////////////////////////
/// @return A boolean value indicating the active state of the GameObject.
/////////////////////////////////////////////////////////////////////////////
bool GCGameObject::IsActive() const { return m_active; }

////////////////////////////////////////////////////////////////////
/// @return A string value indicating the name of the GameObject.
////////////////////////////////////////////////////////////////////
const char* GCGameObject::GetName() const { return m_name; }

///////////////////////////////////////////////////////////////////
/// @return A string value indicating the tag of the GameObject.
///////////////////////////////////////////////////////////////////
const char* GCGameObject::GetTag() const { return m_tag; }

/////////////////////////////////////////////////////////////////
/// @return An integer indicating the layer of the GameObject.
/////////////////////////////////////////////////////////////////
int GCGameObject::GetLayer() const { return m_layer; }



///////////////////////////////////////////////////////////
/// @brief Removes a component from this GameObject.
/// 
/// @param type The type of the component to be removed.
///////////////////////////////////////////////////////////
void GCGameObject::RemoveComponent( int type )
{
    Component* component;
    if ( m_componentsList.Find( type, component ) == true )
    {
        delete component;
        m_componentsList.Remove( type );
    }
    // todo Try to think of another way to store components
}

////////////////////////////////////////////////////////////
/// @brief Removes every components from this GameObject.
////////////////////////////////////////////////////////////
void GCGameObject::ClearComponents()
{
    for ( int i = 1; i <= 7; i++ ) RemoveComponent( i ); //! Limited to 7 components
    // todo Try to think of another way to store components
}