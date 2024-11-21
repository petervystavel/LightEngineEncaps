#include "GCEngine.h"

CREATE_SCRIPT_START( HpText )
public:
	void Update() override;
	void SetPlayer( GCGameObject* pPlayer ) { m_pPlayer = pPlayer; }

private:
	GCGameObject* m_pPlayer;

CREATE_SCRIPT_END