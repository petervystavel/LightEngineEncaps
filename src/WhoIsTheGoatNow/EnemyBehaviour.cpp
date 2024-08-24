#include "pch.h"
#include "EnemyBehaviour.h"

void GCScriptEnemyBehaviour::CopyTo(GCComponent * pDestination)
{
    GCComponent::CopyTo(pDestination);
    GCScriptEnemyBehaviour * pNewComponent = static_cast<GCScriptEnemyBehaviour*>(pDestination);
}

void GCScriptEnemyBehaviour::Start()
{
	m_hp = 1;
    m_speed = 0.03f;
    Spawn();
}

void GCScriptEnemyBehaviour::FixedUpdate()
{
    if (m_pTarget != NULL)
    {
        GCVEC3 direction = m_pTarget->m_transform.m_position;
        direction -= m_pGameObject->m_transform.m_position;
        direction.Normalize();
        direction *= m_speed;

        m_pGameObject->m_transform.Translate(direction);
    }
}

void GCScriptEnemyBehaviour::Spawn()
{
    float x, y;
    
    // Get the half dimensions of the screen
    float halfWidth = GC::GetActiveScene()->GetMainCamera()->GetViewWidth() / 2.0f;
    float halfHeight = GC::GetActiveScene()->GetMainCamera()->GetViewHeight() / 2.0f;

    // Randomly choose if x will be in the range [-15, -10] or [10, 15]
    if (rand() % 2 == 0) {
        // x is in the range [-15, -10]
        x = -15.0f + static_cast<float>(rand()) / RAND_MAX * 5.0f;
    }
    else {
        // x is in the range [10, 15]
        x = 10.0f + static_cast<float>(rand()) / RAND_MAX * 5.0f;
    }

    // Randomly choose if y will be in the range [-15, -10] or [10, 15]
    if (rand() % 2 == 0) {
        // y is in the range [-15, -10]
        y = -15.0f + static_cast<float>(rand()) / RAND_MAX * 5.0f;
    }
    else {
        // y is in the range [10, 15]
        y = 15.0f + static_cast<float>(rand()) / RAND_MAX * 5.0f;
    }

    //Apply to transform
    m_pGameObject->m_transform.Translate(GCVEC3(x, y, 0));

}

void GCScriptEnemyBehaviour::Die()
{
    m_pGameObject->Destroy();
}