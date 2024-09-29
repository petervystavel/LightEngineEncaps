#include "pch.h"
#include "EnemySpawner.h"

void GCScriptEnemySpawner::CopyTo(GCComponent * pDestination)
{
	GCComponent::CopyTo(pDestination);
	GCScriptEnemySpawner * pNewComponent = static_cast<GCScriptEnemySpawner*>(pDestination);
}

void GCScriptEnemySpawner::Start()
{
	m_counter = 0.0f;
}

void GCScriptEnemySpawner::FixedUpdate()
{
	SpawnEnemies(GC::GetActiveTimer()->FixedDeltaTime());
}

void GCScriptEnemySpawner::SpawnEnemies(float deltaTime)
{
	m_counter += deltaTime;
	if (m_counter > 20.0f)
	{
		int randomEnemy = rand() % 10;
		int enemyId = 0;
		if (randomEnemy >= 3)
			enemyId = 1;
		m_counter = 0.0f;
		GCGameObject* newEnemy = m_pEnemies[enemyId]->Duplicate();
		newEnemy->Activate();
	}
}

