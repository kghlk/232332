/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <antibot/antibot_data.h>

#include <base/dbg.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/vmath.h>
#include <game/animation.h>
#include <engine/map.h>
#include <engine/shared/config.h>

#include <game/collision.h>
#include <game/layers.h>
#include <game/mapitems.h>
#include <base/tl/array.h>
#include <cmath>
#include "gamecore.h"

vec2 ClampVel(int MoveRestriction, vec2 Vel)
{
	if(Vel.x > 0 && (MoveRestriction & CANTMOVE_RIGHT))
	{
		Vel.x = 0;
	}
	if(Vel.x < 0 && (MoveRestriction & CANTMOVE_LEFT))
	{
		Vel.x = 0;
	}
	if(Vel.y > 0 && (MoveRestriction & CANTMOVE_DOWN))
	{
		Vel.y = 0;
	}
	if(Vel.y < 0 && (MoveRestriction & CANTMOVE_UP))
	{
		Vel.y = 0;
	}
	return Vel;
}

CCollision::CCollision()
{
	m_pDoor = nullptr;
	Unload();
}

CCollision::~CCollision()
{
	Unload();
}

void CCollision::Init(class CLayers *pLayers)
{
	Unload();

	m_pLayers = pLayers;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));

	if(m_pLayers->TeleLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->TeleLayer()->m_Tele);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CTeleTile))
			m_pTele = static_cast<CTeleTile *>(m_pLayers->Map()->GetData(m_pLayers->TeleLayer()->m_Tele));
	}

	if(m_pLayers->SpeedupLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->SpeedupLayer()->m_Speedup);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CSpeedupTile))
			m_pSpeedup = static_cast<CSpeedupTile *>(m_pLayers->Map()->GetData(m_pLayers->SpeedupLayer()->m_Speedup));
	}

	if(m_pLayers->SwitchLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->SwitchLayer()->m_Switch);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CSwitchTile))
			m_pSwitch = static_cast<CSwitchTile *>(m_pLayers->Map()->GetData(m_pLayers->SwitchLayer()->m_Switch));

		m_pDoor = new CDoorTile[m_Width * m_Height];
		mem_zero(m_pDoor, (size_t)m_Width * m_Height * sizeof(CDoorTile));
	}

	if(m_pLayers->TuneLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->TuneLayer()->m_Tune);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CTuneTile))
			m_pTune = static_cast<CTuneTile *>(m_pLayers->Map()->GetData(m_pLayers->TuneLayer()->m_Tune));
	}

	if(m_pLayers->FrontLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->FrontLayer()->m_Front);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CTile))
			m_pFront = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->FrontLayer()->m_Front));
	}

	for(int i = 0; i < m_Width * m_Height; i++)
	{
		int Index;
		if(m_pSwitch)
		{
			if(m_pSwitch[i].m_Number > m_HighestSwitchNumber)
				m_HighestSwitchNumber = m_pSwitch[i].m_Number;

			if(m_pSwitch[i].m_Number)
				m_pDoor[i].m_Number = m_pSwitch[i].m_Number;
			else
				m_pDoor[i].m_Number = 0;

			Index = m_pSwitch[i].m_Type;

			if(Index <= TILE_NPH_ENABLE)
			{
				if((Index >= TILE_JUMP && Index <= TILE_SUBTRACT_TIME) || Index == TILE_ALLOW_TELE_GUN || Index == TILE_ALLOW_BLUE_TELE_GUN)
					m_pSwitch[i].m_Type = Index;
				else
					m_pSwitch[i].m_Type = 0;
			}
		}
	}

	if(m_pTele)
	{
		for(int i = 0; i < m_Width * m_Height; i++)
		{
			int Number = m_pTele[i].m_Number;
			int Type = m_pTele[i].m_Type;
			if(Number > 0)
			{
				if(Type == TILE_TELEIN)
				{
					m_TeleIns[Number - 1].emplace_back(i % m_Width * 32.0f + 16.0f, i / m_Width * 32.0f + 16.0f);
				}
				else if(Type == TILE_TELEOUT)
				{
					m_TeleOuts[Number - 1].emplace_back(i % m_Width * 32.0f + 16.0f, i / m_Width * 32.0f + 16.0f);
				}
				else if(Type == TILE_TELECHECKOUT)
				{
					m_TeleCheckOuts[Number - 1].emplace_back(i % m_Width * 32.0f + 16.0f, i / m_Width * 32.0f + 16.0f);
				}
				else if(Type)
				{
					m_TeleOthers[Number - 1].emplace_back(i % m_Width * 32.0f + 16.0f, i / m_Width * 32.0f + 16.0f);
				}
			}
		}
	}
}

void CCollision::Unload()
{
	m_pTiles = nullptr;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = nullptr;

	m_HighestSwitchNumber = 0;

	m_TeleIns.clear();
	m_TeleOuts.clear();
	m_TeleCheckOuts.clear();
	m_TeleOthers.clear();

	m_pTele = nullptr;
	m_pSpeedup = nullptr;
	m_pFront = nullptr;
	m_pSwitch = nullptr;
	m_pTune = nullptr;
	delete[] m_pDoor;
	m_pDoor = nullptr;
}

void CCollision::FillAntibot(CAntibotMapData *pMapData) const
{
	pMapData->m_Width = m_Width;
	pMapData->m_Height = m_Height;
	pMapData->m_pTiles = (unsigned char *)malloc((size_t)m_Width * m_Height);
	for(int i = 0; i < m_Width * m_Height; i++)
	{
		pMapData->m_pTiles[i] = 0;
		if(m_pTiles[i].m_Index >= TILE_SOLID && m_pTiles[i].m_Index <= TILE_NOLASER)
		{
			pMapData->m_pTiles[i] = m_pTiles[i].m_Index;
		}
	}
}

enum
{
	MR_DIR_HERE = 0,
	MR_DIR_RIGHT,
	MR_DIR_DOWN,
	MR_DIR_LEFT,
	MR_DIR_UP,
	NUM_MR_DIRS
};

static int GetMoveRestrictionsRaw(int Direction, int Tile, int Flags)
{
	Flags = Flags & (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE);
	switch(Tile)
	{
	case TILE_STOP:
		switch(Flags)
		{
		case ROTATION_0: return CANTMOVE_DOWN;
		case ROTATION_90: return CANTMOVE_LEFT;
		case ROTATION_180: return CANTMOVE_UP;
		case ROTATION_270: return CANTMOVE_RIGHT;

		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_0): return CANTMOVE_UP;
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_90): return CANTMOVE_RIGHT;
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_180): return CANTMOVE_DOWN;
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_270): return CANTMOVE_LEFT;
		}
		break;
	case TILE_STOPS:
		switch(Flags)
		{
		case ROTATION_0:
		case ROTATION_180:
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_0):
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_180):
			return CANTMOVE_DOWN | CANTMOVE_UP;
		case ROTATION_90:
		case ROTATION_270:
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_90):
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_270):
			return CANTMOVE_LEFT | CANTMOVE_RIGHT;
		}
		break;
	case TILE_STOPA:
		return CANTMOVE_LEFT | CANTMOVE_RIGHT | CANTMOVE_UP | CANTMOVE_DOWN;
	}
	return 0;
}

static int GetMoveRestrictionsMask(int Direction)
{
	switch(Direction)
	{
	case MR_DIR_HERE: return 0;
	case MR_DIR_RIGHT: return CANTMOVE_RIGHT;
	case MR_DIR_DOWN: return CANTMOVE_DOWN;
	case MR_DIR_LEFT: return CANTMOVE_LEFT;
	case MR_DIR_UP: return CANTMOVE_UP;
	default: dbg_assert_failed("Invalid Direction: %d", Direction);
	}
}

static int GetMoveRestrictions(int Direction, int Tile, int Flags)
{
	int Result = GetMoveRestrictionsRaw(Direction, Tile, Flags);
	// Generally, stoppers only have an effect if they block us from moving
	// *onto* them. The one exception is one-way blockers, they can also
	// block us from moving if we're on top of them.
	if(Direction == MR_DIR_HERE && Tile == TILE_STOP)
	{
		return Result;
	}
	return Result & GetMoveRestrictionsMask(Direction);
}

int CCollision::GetMoveRestrictions(CALLBACK_SWITCHACTIVE pfnSwitchActive, void *pUser, vec2 Pos, float Distance, int OverrideCenterTileIndex) const
{
	static const vec2 DIRECTIONS[NUM_MR_DIRS] =
		{
			vec2(0, 0),
			vec2(1, 0),
			vec2(0, 1),
			vec2(-1, 0),
			vec2(0, -1)};
	dbg_assert(0.0f <= Distance && Distance <= 32.0f, "Invalid Distance: %f", Distance);
	int Restrictions = 0;
	for(int d = 0; d < NUM_MR_DIRS; d++)
	{
		vec2 ModPos = Pos + DIRECTIONS[d] * Distance;
		int ModMapIndex = GetPureMapIndex(ModPos);
		if(d == MR_DIR_HERE && OverrideCenterTileIndex >= 0)
		{
			ModMapIndex = OverrideCenterTileIndex;
		}
		for(int Front = 0; Front < 2; Front++)
		{
			int Tile;
			int Flags;
			if(!Front)
			{
				Tile = GetTileIndex(ModMapIndex);
				Flags = GetTileFlags(ModMapIndex);
			}
			else
			{
				Tile = GetFrontTileIndex(ModMapIndex);
				Flags = GetFrontTileFlags(ModMapIndex);
			}
			Restrictions |= ::GetMoveRestrictions(d, Tile, Flags);
		}
		if(pfnSwitchActive)
		{
			CDoorTile DoorTile;
			GetDoorTile(ModMapIndex, &DoorTile);
			if(in_range(DoorTile.m_Number, 0, m_HighestSwitchNumber) &&
				pfnSwitchActive(DoorTile.m_Number, pUser))
			{
				Restrictions |= ::GetMoveRestrictions(d, DoorTile.m_Index, DoorTile.m_Flags);
			}
		}
	}
	return Restrictions;
}

int CCollision::GetTile(int x, int y) const
{
	if(!m_pTiles)
		return 0;

	int Nx = std::clamp(x / 32, 0, m_Width - 1);
	int Ny = std::clamp(y / 32, 0, m_Height - 1);
	const int Index = Ny * m_Width + Nx;

	if(m_pTiles[Index].m_Index >= TILE_SOLID && m_pTiles[Index].m_Index <= TILE_NOLASER)
		return m_pTiles[Index].m_Index;
	return 0;
}

// TODO: rewrite this smarter!
int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance + 1);
	vec2 Last = Pos0;
	for(int i = 0; i <= End; i++)
	{
		float a = i / (float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		// Temporary position for checking collision
		int ix = round_to_int(Pos.x);
		int iy = round_to_int(Pos.y);

		if(CheckPoint(ix, iy))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(ix, iy);
		}

		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectLineTeleHook(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int *pTeleNr) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance + 1);
	vec2 Last = Pos0;
	int dx = 0, dy = 0; // Offset for checking the "through" tile
	ThroughOffset(Pos0, Pos1, &dx, &dy);
	for(int i = 0; i <= End; i++)
	{
		float a = i / (float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		// Temporary position for checking collision
		int ix = round_to_int(Pos.x);
		int iy = round_to_int(Pos.y);

		int Index = GetPureMapIndex(Pos);
		if(pTeleNr)
		{
			if(g_Config.m_SvOldTeleportHook)
				*pTeleNr = IsTeleport(Index);
			else
				*pTeleNr = IsTeleportHook(Index);
		}
		if(pTeleNr && *pTeleNr)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return TILE_TELEINHOOK;
		}

		int Hit = 0;
		if(CheckPoint(ix, iy))
		{
			if(!IsThrough(ix, iy, dx, dy, Pos0, Pos1))
				Hit = GetCollisionAt(ix, iy);
		}
		else if(IsHookBlocker(ix, iy, Pos0, Pos1))
		{
			Hit = TILE_NOHOOK;
		}
		if(Hit)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return Hit;
		}

		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectLineTeleWeapon(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int *pTeleNr) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance + 1);
	vec2 Last = Pos0;
	for(int i = 0; i <= End; i++)
	{
		float a = i / (float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		// Temporary position for checking collision
		int ix = round_to_int(Pos.x);
		int iy = round_to_int(Pos.y);

		int Index = GetPureMapIndex(Pos);
		if(pTeleNr)
		{
			if(g_Config.m_SvOldTeleportWeapons)
				*pTeleNr = IsTeleport(Index);
			else
				*pTeleNr = IsTeleportWeapon(Index);
		}
		if(pTeleNr && *pTeleNr)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return TILE_TELEINWEAPON;
		}

		if(CheckPoint(ix, iy))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(ix, iy);
		}

		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

// TODO: OPT: rewrite this smarter!
void CCollision::MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces) const
{
	if(pBounces)
		*pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if(CheckPoint(Pos + Vel))
	{
		int Affected = 0;
		if(CheckPoint(Pos.x + Vel.x, Pos.y))
		{
			pInoutVel->x *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(CheckPoint(Pos.x, Pos.y + Vel.y))
		{
			pInoutVel->y *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(Affected == 0)
		{
			pInoutVel->x *= -Elasticity;
			pInoutVel->y *= -Elasticity;
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
}

bool CCollision::TestBox(vec2 Pos, vec2 Size) const
{
        Size *= 0.5f;
        if(CheckPoint(Pos.x - Size.x, Pos.y - Size.y))
                return true;
        if(CheckPoint(Pos.x + Size.x, Pos.y - Size.y))
                return true;
        if(CheckPoint(Pos.x - Size.x, Pos.y + Size.y))
                return true;
        if(CheckPoint(Pos.x + Size.x, Pos.y + Size.y))
                return true;
        return false;
}

bool CCollision::TestBoxSize(vec2 Pos, vec2 Size) const
{
	Size *= 0.5f;
	int StartX = (int)floor((Pos.x - Size.x) / 32.0f);
	int EndX = (int)floor((Pos.x + Size.x) / 32.0f);
	int StartY = (int)floor((Pos.y - Size.y) / 32.0f);
	int EndY = (int)floor((Pos.y + Size.y) / 32.0f);

	for(int y = StartY; y <= EndY; y++)
	{
		for(int x = StartX; x <= EndX; x++)
		{
			if(GetCollisionAt(x * 32.0f + 16.0f, y * 32.0f + 16.0f))
				return true;
		}
	}
	return false;
}

vec2 CCollision::ctile(vec2 Pos, vec2 Size) const
{
	Size *= 0.5f;
	int StartX = (int)floor((Pos.x - Size.x) / 32.0f);
	int EndX = (int)floor((Pos.x + Size.x) / 32.0f);
	int StartY = (int)floor((Pos.y - Size.y) / 32.0f);
	int EndY = (int)floor((Pos.y + Size.y) / 32.0f);

	vec2 ClosestTile = Pos;
	float MinDist = 1000000.0f;

	for(int y = StartY; y <= EndY; y++)
	{
		for(int x = StartX; x <= EndX; x++)
		{
			vec2 TileCenter = vec2(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
			if(GetCollisionAt(TileCenter.x, TileCenter.y))
			{
				float d = distance(Pos, TileCenter);
				if(d < MinDist)
				{
					MinDist = d;
					ClosestTile = TileCenter;
				}
			}
		}
	}
	return ClosestTile;
}

int CCollision::GetQuadOrder(CQuad TargetQuad)
{
    CMapItemLayerQuads *pQLayer = nullptr;

    char aLayerName[12];
    for(int l = 0; l < m_pLayers->ZoneGroup()->m_NumLayers; l++)
    {
        CMapItemLayer *pLayer = m_pLayers->GetLayer(m_pLayers->ZoneGroup()->m_StartLayer + l);

        if(pLayer->m_Type == LAYERTYPE_QUADS)
        {
            CMapItemLayerQuads *pQLayerL = (CMapItemLayerQuads *)pLayer;
            IntsToStr(pQLayerL->m_aName, std::size(pQLayerL->m_aName), aLayerName, std::size(aLayerName));
            if(str_comp("tile", aLayerName) == 0)
                pQLayer = pQLayerL;
        }
    }

    if(!pQLayer || !m_pLayers)
        return -1;

    const CQuad *pQuads = (const CQuad *)m_pLayers->Map()->GetDataSwapped(pQLayer->m_Data);

    for(int i = 0; i < pQLayer->m_NumQuads; i++) {
        if(memcmp(&pQuads[i], &TargetQuad, sizeof(CQuad)) == 0) {
            return i;
        }
    }

    return -1;
}

bool CCollision::IsOnGround(vec2 Pos, float Size) const
{
	if(CheckPoint(Pos.x + Size / 2, Pos.y + Size / 2 + 5))
		return true;
	if(CheckPoint(Pos.x - Size / 2, Pos.y + Size / 2 + 5))
		return true;

	return false;
}

void CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, vec2 Elasticity, bool *pGrounded) const
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;

	float Distance = length(Vel);
	int Max = (int)Distance;

	if(Distance > 0.00001f)
	{
		float Fraction = 1.0f / (float)(Max + 1);
		float ElasticityX = std::clamp(Elasticity.x, -1.0f, 1.0f);
		float ElasticityY = std::clamp(Elasticity.y, -1.0f, 1.0f);

		for(int i = 0; i <= Max; i++)
		{
			// Early break as optimization to stop checking for collisions for
			// large distances after the obstacles we have already hit reduced
			// our speed to exactly 0.
			if(Vel == vec2(0, 0))
			{
				break;
			}

			vec2 NewPos = Pos + Vel * Fraction; // TODO: this row is not nice

			// Fraction can be very small and thus the calculation has no effect, no
			// reason to continue calculating.
			if(NewPos == Pos)
			{
				break;
			}

			if(TestBox(vec2(NewPos.x, NewPos.y), Size))
			{
				int Hits = 0;

				if(TestBox(vec2(Pos.x, NewPos.y), Size))
				{
					if(pGrounded && ElasticityY > 0 && Vel.y > 0)
						*pGrounded = true;
					NewPos.y = Pos.y;
					Vel.y *= -ElasticityY;
					Hits++;
				}

				if(TestBox(vec2(NewPos.x, Pos.y), Size))
				{
					NewPos.x = Pos.x;
					Vel.x *= -ElasticityX;
					Hits++;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if(Hits == 0)
				{
					if(pGrounded && ElasticityY > 0 && Vel.y > 0)
						*pGrounded = true;
					NewPos.y = Pos.y;
					Vel.y *= -ElasticityY;
					NewPos.x = Pos.x;
					Vel.x *= -ElasticityX;
				}
			}

			Pos = NewPos;
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}

// DDRace

int CCollision::IsSolid(int x, int y) const
{
	const int Index = GetTile(x, y);
	return Index == TILE_SOLID || Index == TILE_NOHOOK;
}

bool CCollision::IsThrough(int x, int y, int OffsetX, int OffsetY, vec2 Pos0, vec2 Pos1) const
{
	const int Index = GetPureMapIndex(x, y);
	if(m_pFront && (m_pFront[Index].m_Index == TILE_THROUGH_ALL || m_pFront[Index].m_Index == TILE_THROUGH_CUT))
		return true;
	if(m_pFront && m_pFront[Index].m_Index == TILE_THROUGH_DIR && ((m_pFront[Index].m_Flags == ROTATION_0 && Pos0.y > Pos1.y) || (m_pFront[Index].m_Flags == ROTATION_90 && Pos0.x < Pos1.x) || (m_pFront[Index].m_Flags == ROTATION_180 && Pos0.y < Pos1.y) || (m_pFront[Index].m_Flags == ROTATION_270 && Pos0.x > Pos1.x)))
		return true;
	const int OffsetIndex = GetPureMapIndex(x + OffsetX, y + OffsetY);
	return m_pTiles[OffsetIndex].m_Index == TILE_THROUGH || (m_pFront && m_pFront[OffsetIndex].m_Index == TILE_THROUGH);
}

bool CCollision::IsHookBlocker(int x, int y, vec2 Pos0, vec2 Pos1) const
{
	const int Index = GetPureMapIndex(x, y);
	if(m_pTiles[Index].m_Index == TILE_THROUGH_ALL || (m_pFront && m_pFront[Index].m_Index == TILE_THROUGH_ALL))
		return true;
	if(m_pTiles[Index].m_Index == TILE_THROUGH_DIR && ((m_pTiles[Index].m_Flags == ROTATION_0 && Pos0.y < Pos1.y) ||
								  (m_pTiles[Index].m_Flags == ROTATION_90 && Pos0.x > Pos1.x) ||
								  (m_pTiles[Index].m_Flags == ROTATION_180 && Pos0.y > Pos1.y) ||
								  (m_pTiles[Index].m_Flags == ROTATION_270 && Pos0.x < Pos1.x)))
		return true;
	if(m_pFront && m_pFront[Index].m_Index == TILE_THROUGH_DIR && ((m_pFront[Index].m_Flags == ROTATION_0 && Pos0.y < Pos1.y) || (m_pFront[Index].m_Flags == ROTATION_90 && Pos0.x > Pos1.x) || (m_pFront[Index].m_Flags == ROTATION_180 && Pos0.y > Pos1.y) || (m_pFront[Index].m_Flags == ROTATION_270 && Pos0.x < Pos1.x)))
		return true;
	return false;
}

int CCollision::IsWallJump(int Index) const
{
	if(Index < 0)
		return 0;

	return m_pTiles[Index].m_Index == TILE_WALLJUMP;
}

int CCollision::IsNoLaser(int x, int y) const
{
	return (CCollision::GetTile(x, y) == TILE_NOLASER);
}

int CCollision::IsFrontNoLaser(int x, int y) const
{
	return (CCollision::GetFrontTile(x, y) == TILE_NOLASER);
}

int CCollision::IsTeleport(int Index) const
{
	if(Index < 0 || !m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEIN)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsEvilTeleport(int Index) const
{
	if(Index < 0)
		return 0;
	if(!m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEINEVIL)
		return m_pTele[Index].m_Number;

	return 0;
}

bool CCollision::IsCheckTeleport(int Index) const
{
	if(Index < 0 || !m_pTele)
		return false;
	return m_pTele[Index].m_Type == TILE_TELECHECKIN;
}

bool CCollision::IsCheckEvilTeleport(int Index) const
{
	if(Index < 0 || !m_pTele)
		return false;
	return m_pTele[Index].m_Type == TILE_TELECHECKINEVIL;
}

int CCollision::IsTeleCheckpoint(int Index) const
{
	if(Index < 0)
		return 0;

	if(!m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELECHECK)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsTeleportWeapon(int Index) const
{
	if(Index < 0 || !m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEINWEAPON)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsTeleportHook(int Index) const
{
	if(Index < 0 || !m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEINHOOK)
		return m_pTele[Index].m_Number;

	return 0;
}

bool CCollision::IsSpeedup(int Index) const
{
	dbg_assert(Index >= 0, "Invalid speedup index %d", Index);
	return m_pSpeedup && m_pSpeedup[Index].m_Force > 0;
}

int CCollision::IsTune(int Index) const
{
	if(Index < 0 || !m_pTune)
		return 0;

	if(m_pTune[Index].m_Type)
		return m_pTune[Index].m_Number;

	return 0;
}

void CCollision::GetSpeedup(int Index, vec2 *pDir, int *pForce, int *pMaxSpeed, int *pType) const
{
	if(Index < 0 || !m_pSpeedup)
		return;
	float Angle = m_pSpeedup[Index].m_Angle * (pi / 180.0f);
	*pForce = m_pSpeedup[Index].m_Force;
	*pType = m_pSpeedup[Index].m_Type;
	*pDir = direction(Angle);
	if(pMaxSpeed)
		*pMaxSpeed = m_pSpeedup[Index].m_MaxSpeed;
}

int CCollision::GetSwitchType(int Index) const
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0)
		return m_pSwitch[Index].m_Type;

	return 0;
}

int CCollision::GetSwitchNumber(int Index) const
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0 && m_pSwitch[Index].m_Number > 0)
		return m_pSwitch[Index].m_Number;

	return 0;
}

int CCollision::GetSwitchDelay(int Index) const
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0)
		return m_pSwitch[Index].m_Delay;

	return 0;
}

int CCollision::MoverSpeed(int x, int y, vec2 *pSpeed) const
{
	int Nx = std::clamp(x / 32, 0, m_Width - 1);
	int Ny = std::clamp(y / 32, 0, m_Height - 1);
	int Index = m_pTiles[Ny * m_Width + Nx].m_Index;

	if(Index != TILE_CP && Index != TILE_CP_F)
	{
		return 0;
	}

	vec2 Target;
	switch(m_pTiles[Ny * m_Width + Nx].m_Flags)
	{
	case ROTATION_0:
		Target.x = 0.0f;
		Target.y = -4.0f;
		break;
	case ROTATION_90:
		Target.x = 4.0f;
		Target.y = 0.0f;
		break;
	case ROTATION_180:
		Target.x = 0.0f;
		Target.y = 4.0f;
		break;
	case ROTATION_270:
		Target.x = -4.0f;
		Target.y = 0.0f;
		break;
	default:
		Target = vec2(0.0f, 0.0f);
		break;
	}
	if(Index == TILE_CP_F)
	{
		Target *= 4.0f;
	}
	*pSpeed = Target;
	return Index;
}

int CCollision::GetPureMapIndex(float x, float y) const
{
	int Nx = std::clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = std::clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return Ny * m_Width + Nx;
}

bool CCollision::TileExists(int Index) const
{
	if(Index < 0)
		return false;

	if((m_pTiles[Index].m_Index >= TILE_FREEZE && m_pTiles[Index].m_Index <= TILE_TELE_LASER_DISABLE) || (m_pTiles[Index].m_Index >= TILE_LFREEZE && m_pTiles[Index].m_Index <= TILE_LUNFREEZE))
		return true;
	if(m_pFront && ((m_pFront[Index].m_Index >= TILE_FREEZE && m_pFront[Index].m_Index <= TILE_TELE_LASER_DISABLE) || (m_pFront[Index].m_Index >= TILE_LFREEZE && m_pFront[Index].m_Index <= TILE_LUNFREEZE)))
		return true;
	if(m_pTele && (m_pTele[Index].m_Type == TILE_TELEIN || m_pTele[Index].m_Type == TILE_TELEINEVIL || m_pTele[Index].m_Type == TILE_TELECHECKINEVIL || m_pTele[Index].m_Type == TILE_TELECHECK || m_pTele[Index].m_Type == TILE_TELECHECKIN))
		return true;
	if(m_pSpeedup && m_pSpeedup[Index].m_Force > 0)
		return true;
	if(m_pDoor && m_pDoor[Index].m_Index)
		return true;
	if(m_pSwitch && m_pSwitch[Index].m_Type)
		return true;
	if(m_pTune && m_pTune[Index].m_Type)
		return true;
	return TileExistsNext(Index);
}

bool CCollision::TileExistsNext(int Index) const
{
	if(Index < 0)
		return false;
	int TileOnTheLeft = (Index - 1 > 0) ? Index - 1 : Index;
	int TileOnTheRight = (Index + 1 < m_Width * m_Height) ? Index + 1 : Index;
	int TileBelow = (Index + m_Width < m_Width * m_Height) ? Index + m_Width : Index;
	int TileAbove = (Index - m_Width > 0) ? Index - m_Width : Index;

	if((m_pTiles[TileOnTheRight].m_Index == TILE_STOP && m_pTiles[TileOnTheRight].m_Flags == ROTATION_270) || (m_pTiles[TileOnTheLeft].m_Index == TILE_STOP && m_pTiles[TileOnTheLeft].m_Flags == ROTATION_90))
		return true;
	if((m_pTiles[TileBelow].m_Index == TILE_STOP && m_pTiles[TileBelow].m_Flags == ROTATION_0) || (m_pTiles[TileAbove].m_Index == TILE_STOP && m_pTiles[TileAbove].m_Flags == ROTATION_180))
		return true;
	if(m_pTiles[TileOnTheRight].m_Index == TILE_STOPA || m_pTiles[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pTiles[TileOnTheRight].m_Index == TILE_STOPS || m_pTiles[TileOnTheLeft].m_Index == TILE_STOPS)))
		return true;
	if(m_pTiles[TileBelow].m_Index == TILE_STOPA || m_pTiles[TileAbove].m_Index == TILE_STOPA || ((m_pTiles[TileBelow].m_Index == TILE_STOPS || m_pTiles[TileAbove].m_Index == TILE_STOPS) && m_pTiles[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
		return true;
	if(m_pFront)
	{
		if(m_pFront[TileOnTheRight].m_Index == TILE_STOPA || m_pFront[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pFront[TileOnTheRight].m_Index == TILE_STOPS || m_pFront[TileOnTheLeft].m_Index == TILE_STOPS)))
			return true;
		if(m_pFront[TileBelow].m_Index == TILE_STOPA || m_pFront[TileAbove].m_Index == TILE_STOPA || ((m_pFront[TileBelow].m_Index == TILE_STOPS || m_pFront[TileAbove].m_Index == TILE_STOPS) && m_pFront[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
			return true;
		if((m_pFront[TileOnTheRight].m_Index == TILE_STOP && m_pFront[TileOnTheRight].m_Flags == ROTATION_270) || (m_pFront[TileOnTheLeft].m_Index == TILE_STOP && m_pFront[TileOnTheLeft].m_Flags == ROTATION_90))
			return true;
		if((m_pFront[TileBelow].m_Index == TILE_STOP && m_pFront[TileBelow].m_Flags == ROTATION_0) || (m_pFront[TileAbove].m_Index == TILE_STOP && m_pFront[TileAbove].m_Flags == ROTATION_180))
			return true;
	}
	if(m_pDoor)
	{
		if(m_pDoor[TileOnTheRight].m_Index == TILE_STOPA || m_pDoor[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pDoor[TileOnTheRight].m_Index == TILE_STOPS || m_pDoor[TileOnTheLeft].m_Index == TILE_STOPS)))
			return true;
		if(m_pDoor[TileBelow].m_Index == TILE_STOPA || m_pDoor[TileAbove].m_Index == TILE_STOPA || ((m_pDoor[TileBelow].m_Index == TILE_STOPS || m_pDoor[TileAbove].m_Index == TILE_STOPS) && m_pDoor[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
			return true;
		if((m_pDoor[TileOnTheRight].m_Index == TILE_STOP && m_pDoor[TileOnTheRight].m_Flags == ROTATION_270) || (m_pDoor[TileOnTheLeft].m_Index == TILE_STOP && m_pDoor[TileOnTheLeft].m_Flags == ROTATION_90))
			return true;
		if((m_pDoor[TileBelow].m_Index == TILE_STOP && m_pDoor[TileBelow].m_Flags == ROTATION_0) || (m_pDoor[TileAbove].m_Index == TILE_STOP && m_pDoor[TileAbove].m_Flags == ROTATION_180))
			return true;
	}
	return false;
}

int CCollision::GetMapIndex(vec2 Pos) const
{
	int Nx = std::clamp((int)Pos.x / 32, 0, m_Width - 1);
	int Ny = std::clamp((int)Pos.y / 32, 0, m_Height - 1);
	int Index = Ny * m_Width + Nx;

	if(TileExists(Index))
		return Index;
	else
		return -1;
}

std::vector<int> CCollision::GetMapIndices(vec2 PrevPos, vec2 Pos, unsigned MaxIndices) const
{
	std::vector<int> vIndices;
	float d = distance(PrevPos, Pos);
	int End(d + 1);
	if(!d)
	{
		int Nx = std::clamp((int)Pos.x / 32, 0, m_Width - 1);
		int Ny = std::clamp((int)Pos.y / 32, 0, m_Height - 1);
		int Index = Ny * m_Width + Nx;

		if(TileExists(Index))
		{
			vIndices.push_back(Index);
			return vIndices;
		}
		else
			return vIndices;
	}
	else
	{
		int LastIndex = 0;
		for(int i = 0; i < End; i++)
		{
			float a = i / d;
			vec2 Tmp = mix(PrevPos, Pos, a);
			int Nx = std::clamp((int)Tmp.x / 32, 0, m_Width - 1);
			int Ny = std::clamp((int)Tmp.y / 32, 0, m_Height - 1);
			int Index = Ny * m_Width + Nx;
			if(TileExists(Index) && LastIndex != Index)
			{
				if(MaxIndices && vIndices.size() > MaxIndices)
					return vIndices;
				vIndices.push_back(Index);
				LastIndex = Index;
			}
		}

		return vIndices;
	}
}

vec2 CCollision::GetPos(int Index) const
{
	if(Index < 0)
		return vec2(0, 0);

	int x = Index % m_Width;
	int y = Index / m_Width;
	return vec2(x * 32 + 16, y * 32 + 16);
}

int CCollision::GetTileIndex(int Index) const
{
	if(Index < 0)
		return 0;
	return m_pTiles[Index].m_Index;
}

int CCollision::GetFrontTileIndex(int Index) const
{
	if(Index < 0 || !m_pFront)
		return 0;
	return m_pFront[Index].m_Index;
}

int CCollision::GetTileFlags(int Index) const
{
	if(Index < 0)
		return 0;
	return m_pTiles[Index].m_Flags;
}

int CCollision::GetFrontTileFlags(int Index) const
{
	if(Index < 0 || !m_pFront)
		return 0;
	return m_pFront[Index].m_Flags;
}

int CCollision::GetIndex(int Nx, int Ny) const
{
	return m_pTiles[Ny * m_Width + Nx].m_Index;
}

int CCollision::GetIndex(vec2 PrevPos, vec2 Pos) const
{
	float Distance = distance(PrevPos, Pos);

	if(!Distance)
	{
		int Nx = std::clamp((int)Pos.x / 32, 0, m_Width - 1);
		int Ny = std::clamp((int)Pos.y / 32, 0, m_Height - 1);

		if((m_pTele) ||
			(m_pSpeedup && m_pSpeedup[Ny * m_Width + Nx].m_Force > 0))
		{
			return Ny * m_Width + Nx;
		}
	}

	const int DistanceRounded = std::ceil(Distance);
	for(int i = 0; i < DistanceRounded; i++)
	{
		float a = (float)i / Distance;
		vec2 Tmp = mix(PrevPos, Pos, a);
		int Nx = std::clamp((int)Tmp.x / 32, 0, m_Width - 1);
		int Ny = std::clamp((int)Tmp.y / 32, 0, m_Height - 1);
		if((m_pTele) ||
			(m_pSpeedup && m_pSpeedup[Ny * m_Width + Nx].m_Force > 0))
		{
			return Ny * m_Width + Nx;
		}
	}

	return -1;
}

int CCollision::GetFrontIndex(int Nx, int Ny) const
{
	if(!m_pFront)
		return 0;
	return m_pFront[Ny * m_Width + Nx].m_Index;
}

int CCollision::GetFrontTile(int x, int y) const
{
	if(!m_pFront)
		return 0;
	int Nx = std::clamp(x / 32, 0, m_Width - 1);
	int Ny = std::clamp(y / 32, 0, m_Height - 1);
	if(m_pFront[Ny * m_Width + Nx].m_Index == TILE_DEATH || m_pFront[Ny * m_Width + Nx].m_Index == TILE_NOLASER)
		return m_pFront[Ny * m_Width + Nx].m_Index;
	else
		return 0;
}

int CCollision::Entity(int x, int y, int Layer) const
{
	if(x < 0 || x >= m_Width || y < 0 || y >= m_Height)
		return 0;

	const int Index = y * m_Width + x;
	switch(Layer)
	{
	case LAYER_GAME:
		return m_pTiles[Index].m_Index - ENTITY_OFFSET;
	case LAYER_FRONT:
		return m_pFront[Index].m_Index - ENTITY_OFFSET;
	case LAYER_SWITCH:
		return m_pSwitch[Index].m_Type - ENTITY_OFFSET;
	case LAYER_TELE:
		return m_pTele[Index].m_Type - ENTITY_OFFSET;
	case LAYER_SPEEDUP:
		return m_pSpeedup[Index].m_Type - ENTITY_OFFSET;
	case LAYER_TUNE:
		return m_pTune[Index].m_Type - ENTITY_OFFSET;
	default:
		dbg_assert_failed("Invalid Layer: %d", Layer);
	}
}

void CCollision::SetCollisionAt(float x, float y, int Index)
{
	int Nx = std::clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = std::clamp(round_to_int(y) / 32, 0, m_Height - 1);

	m_pTiles[Ny * m_Width + Nx].m_Index = Index;
}

void CCollision::SetDoorCollisionAt(float x, float y, int Type, int Flags, int Number)
{
	if(!m_pDoor)
		return;
	int Nx = std::clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = std::clamp(round_to_int(y) / 32, 0, m_Height - 1);

	m_pDoor[Ny * m_Width + Nx].m_Index = Type;
	m_pDoor[Ny * m_Width + Nx].m_Flags = Flags;
	m_pDoor[Ny * m_Width + Nx].m_Number = Number;
}

void CCollision::GetDoorTile(int Index, CDoorTile *pDoorTile) const
{
	if(!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
	{
		pDoorTile->m_Index = 0;
		pDoorTile->m_Flags = 0;
		pDoorTile->m_Number = 0;
		return;
	}
	*pDoorTile = m_pDoor[Index];
}

void ThroughOffset(vec2 Pos0, vec2 Pos1, int *pOffsetX, int *pOffsetY)
{
	float x = Pos0.x - Pos1.x;
	float y = Pos0.y - Pos1.y;
	if(absolute(x) > absolute(y))
	{
		if(x < 0)
		{
			*pOffsetX = -32;
			*pOffsetY = 0;
		}
		else
		{
			*pOffsetX = 32;
			*pOffsetY = 0;
		}
	}
	else
	{
		if(y < 0)
		{
			*pOffsetX = 0;
			*pOffsetY = -32;
		}
		else
		{
			*pOffsetX = 0;
			*pOffsetY = 32;
		}
	}
}

int CCollision::IntersectNoLaser(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float Distance = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	const int DistanceRounded = std::ceil(Distance);
	for(int i = 0; i < DistanceRounded; i++)
	{
		float a = i / Distance;
		vec2 Pos = mix(Pos0, Pos1, a);
		int Nx = std::clamp(round_to_int(Pos.x) / 32, 0, m_Width - 1);
		int Ny = std::clamp(round_to_int(Pos.y) / 32, 0, m_Height - 1);
		if(GetIndex(Nx, Ny) == TILE_SOLID || GetIndex(Nx, Ny) == TILE_NOHOOK || GetIndex(Nx, Ny) == TILE_NOLASER || GetFrontIndex(Nx, Ny) == TILE_NOLASER)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if(GetFrontIndex(Nx, Ny) == TILE_NOLASER)
				return GetFrontCollisionAt(Pos.x, Pos.y);
			else
				return GetCollisionAt(Pos.x, Pos.y);
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectNoLaserNoWalls(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float Distance = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	const int DistanceRounded = std::ceil(Distance);
	for(int i = 0; i < DistanceRounded; i++)
	{
		float a = (float)i / Distance;
		vec2 Pos = mix(Pos0, Pos1, a);
		if(IsNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)) || IsFrontNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if(IsNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)))
				return GetCollisionAt(Pos.x, Pos.y);
			else
				return GetFrontCollisionAt(Pos.x, Pos.y);
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectAir(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float Distance = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	const int DistanceRounded = std::ceil(Distance);
	for(int i = 0; i < DistanceRounded; i++)
	{
		float a = (float)i / Distance;
		vec2 Pos = mix(Pos0, Pos1, a);
		if(IsSolid(round_to_int(Pos.x), round_to_int(Pos.y)) || (!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)) && !GetFrontTile(round_to_int(Pos.x), round_to_int(Pos.y))))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if(!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)) && !GetFrontTile(round_to_int(Pos.x), round_to_int(Pos.y)))
				return -1;
			else if(!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)))
				return GetTile(round_to_int(Pos.x), round_to_int(Pos.y));
			else
				return GetFrontTile(round_to_int(Pos.x), round_to_int(Pos.y));
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IsTimeCheckpoint(int Index) const
{
	if(Index < 0)
		return -1;

	int z = m_pTiles[Index].m_Index;
	if(z >= TILE_TIME_CHECKPOINT_FIRST && z <= TILE_TIME_CHECKPOINT_LAST)
		return z - TILE_TIME_CHECKPOINT_FIRST;
	return -1;
}

int CCollision::IsFrontTimeCheckpoint(int Index) const
{
	if(Index < 0 || !m_pFront)
		return -1;

	int z = m_pFront[Index].m_Index;
	if(z >= TILE_TIME_CHECKPOINT_FIRST && z <= TILE_TIME_CHECKPOINT_LAST)
		return z - TILE_TIME_CHECKPOINT_FIRST;
	return -1;
}

vec2 CCollision::TeleAllGet(int Number, size_t Offset)
{
	if(m_TeleIns.contains(Number))
	{
		if(m_TeleIns[Number].size() > Offset)
			return m_TeleIns[Number][Offset];
		else
			Offset -= m_TeleIns[Number].size();
	}
	if(m_TeleOuts.contains(Number))
	{
		if(m_TeleOuts[Number].size() > Offset)
			return m_TeleOuts[Number][Offset];
		else
			Offset -= m_TeleOuts[Number].size();
	}
	if(m_TeleCheckOuts.contains(Number))
	{
		if(m_TeleCheckOuts[Number].size() > Offset)
			return m_TeleCheckOuts[Number][Offset];
		else
			Offset -= m_TeleCheckOuts[Number].size();
	}
	if(m_TeleOthers.contains(Number))
	{
		if(m_TeleOthers[Number].size() > Offset)
			return m_TeleOthers[Number][Offset];
	}
	return vec2(-1, -1);
}

size_t CCollision::TeleAllSize(int Number)
{
	size_t Total = 0;
	if(m_TeleIns.contains(Number))
		Total += m_TeleIns[Number].size();
	if(m_TeleOuts.contains(Number))
		Total += m_TeleOuts[Number].size();
	if(m_TeleCheckOuts.contains(Number))
		Total += m_TeleCheckOuts[Number].size();
	if(m_TeleOthers.contains(Number))
		Total += m_TeleOthers[Number].size();
	return Total;
}
/* TEEUNIVERSE  BEGIN *************************************************/
int CCollision::GetZoneHandle(const char *pName)
{
	if(!m_pLayers->ZoneGroup())
		return -1;

	int Handle = m_Zones.size();
	m_Zones.add({});

	array<CMapItemLayer *> &LayerList = m_Zones[Handle];

	char aLayerName[12];
	for(int l = 0; l < m_pLayers->ZoneGroup()->m_NumLayers; l++)
	{
		CMapItemLayer *pLayer = m_pLayers->GetLayer(m_pLayers->ZoneGroup()->m_StartLayer + l);

		if(pLayer->m_Type == LAYERTYPE_TILES)
		{
			CMapItemLayerTilemap *pTLayer = (CMapItemLayerTilemap *)pLayer;
			IntsToStr(pTLayer->m_aName, std::size(pTLayer->m_aName), aLayerName, std::size(aLayerName));
			if(str_comp(pName, aLayerName) == 0)
				LayerList.add(pLayer);
		}
		else if(pLayer->m_Type == LAYERTYPE_QUADS)
		{
			CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;
			IntsToStr(pQLayer->m_aName, std::size(pQLayer->m_aName), aLayerName, std::size(aLayerName));
			if(str_comp(pName, aLayerName) == 0)
				LayerList.add(pLayer);
		}
	}

	return Handle;
}

inline bool SameSide(const vec2 &l0, const vec2 &l1, const vec2 &p0, const vec2 &p1)
{
	vec2 l0l1 = l1 - l0;
	vec2 l0p0 = p0 - l0;
	vec2 l0p1 = p1 - l0;

	// This check helps in some cases but we still have fails in some others.
	// The fail is reproducible with infc_provence bells
	if((l0l1 == l0p0) || (l0l1 == l0p1) || (l0p0 == l0p1))
		return false;

	return sign(l0l1.x * l0p0.y - l0l1.y * l0p0.x) == sign(l0l1.x * l0p1.y - l0l1.y * l0p1.x);
}

// t0, t1 and t2 are position of triangle vertices
inline vec3 BarycentricCoordinates(const vec2 &t0, const vec2 &t1, const vec2 &t2, const vec2 &p)
{
	vec2 e0 = t1 - t0;
	vec2 e1 = t2 - t0;
	vec2 e2 = p - t0;

	float d00 = dot(e0, e0);
	float d01 = dot(e0, e1);
	float d11 = dot(e1, e1);
	float d20 = dot(e2, e0);
	float d21 = dot(e2, e1);
	float denom = d00 * d11 - d01 * d01;

	vec3 bary;
	bary.x = (d11 * d20 - d01 * d21) / denom;
	bary.y = (d00 * d21 - d01 * d20) / denom;
	bary.z = 1.0f - bary.x - bary.y;

	return bary;
}

template<typename V>
bool OutOfRange(V value, V q0, V q1, V q2, V q3)
{
	if(q0 > q1)
	{
		if(q2 > q3)
		{
			const V Min = minimum(q1, q3);
			if(value < Min)
				return true;
			const V Max = maximum(q0, q2);
			if(value > Max)
				return true;
		}
		else
		{
			const V Min = minimum(q1, q2);
			if(value < Min)
				return true;
			const V Max = maximum(q0, q3);
			if(value > Max)
				return true;
		}
	}
	else
	{
		// q1 is bigger than q0
		if(q2 > q3)
		{
			const V Min = minimum(q0, q3);
			if(value < Min)
				return true;
			const V Max = maximum(q1, q2);
			if(value > Max)
				return true;
		}
		else
		{
			// q3 is bigger than q2
			const V Min = minimum(q0, q2);
			if(value < Min)
				return true;
			const V Max = maximum(q1, q3);
			if(value > Max)
				return true;
		}
	}

	return false;
}

// t0, t1 and t2 are position of triangle vertices
inline bool InsideTriangle(const vec2 &t0, const vec2 &t1, const vec2 &t2, const vec2 &p)
{
	vec3 bary = BarycentricCoordinates(t0, t1, t2, p);
	return (bary.x >= 0.0f && bary.y >= 0.0f && bary.x + bary.y < 1.0f);
}

// q0, q1, q2 and q3 are position of quad vertices
inline bool InsideQuad(const vec2 &q0, const vec2 &q1, const vec2 &q2, const vec2 &q3, const vec2 &p)
{
	return InsideTriangle(q0, q1, q2, p) || InsideTriangle(q1, q2, q3, p);
#if 0
	// SameSide() check is broken.
	if(SameSide(q1, q2, p, q0))
		return InsideTriangle(q0, q1, q2, p);
	else
		return InsideTriangle(q1, q2, q3, p);
#endif
}

/* TEEUNIVERSE END ****************************************************/

static void Rotate(vec2 *pCenter, vec2 *pPoint, float Rotation)
{
	float x = pPoint->x - pCenter->x;
	float y = pPoint->y - pCenter->y;
	pPoint->x = (x * cosf(Rotation) - y * sinf(Rotation) + pCenter->x);
	pPoint->y = (x * sinf(Rotation) + y * cosf(Rotation) + pCenter->y);
}

struct SAnimationTransformCache
{
	vec2 Position = vec2(0.0f, 0.f);
	float Angle = 0;
	int PosEnv = -1;
};

int CCollision::GetZoneValueAt(int ZoneHandle, vec2 Pos, double time, ZoneData *pData)
{
	if(!m_pLayers->ZoneGroup())
		return 0;

	if(ZoneHandle < 0 || ZoneHandle >= m_Zones.size())
		return 0;

	int Index = 0;
	int ExtraData = 0;

	SAnimationTransformCache AnimationCache;

	const array<CMapItemLayer *> &Zone = m_Zones[ZoneHandle];
	for(int i = 0; i < Zone.size(); i++)
	{
		const CMapItemLayer *pLayer = Zone[i];
		if(pLayer->m_Type == LAYERTYPE_TILES)
		{
			const CMapItemLayerTilemap *pTLayer = (const CMapItemLayerTilemap *)pLayer;
			const CTile *pTiles = (const CTile *)m_pLayers->Map()->GetData(pTLayer->m_Data);

			int Nx = std::clamp(static_cast<int>(Pos.x) / 32, 0, pTLayer->m_Width - 1);
			int Ny = std::clamp(static_cast<int>(Pos.y) / 32, 0, pTLayer->m_Height - 1);

			int TileIndex = (pTiles[Ny * pTLayer->m_Width + Nx].m_Index > 128 ? 0 : pTiles[Ny * pTLayer->m_Width + Nx].m_Index);
			if(TileIndex > 0)
			{
				Index = TileIndex;
				break;
			}
		}
		else if(pLayer->m_Type == LAYERTYPE_QUADS)
		{
			const CMapItemLayerQuads *pQLayer = (const CMapItemLayerQuads *)pLayer;
			const CQuad *pQuads = (const CQuad *)m_pLayers->Map()->GetDataSwapped(pQLayer->m_Data);

			for(int q = 0; q < pQLayer->m_NumQuads; q++)
			{
				vec2 Position(0.0f, 0.0f);
				float Angle = 0.0f;
				if(pQuads[q].m_PosEnv >= 0)
				{
					if(pQuads[q].m_PosEnv != AnimationCache.PosEnv)
					{
						AnimationCache.PosEnv = pQuads[q].m_PosEnv;
						GetAnimationTransform(time, AnimationCache.PosEnv, m_pLayers, AnimationCache.Position, AnimationCache.Angle);
					}

					Position = AnimationCache.Position;
					Angle = AnimationCache.Angle;
				}

				vec2 p0 = Position + vec2(fx2f(pQuads[q].m_aPoints[0].x), fx2f(pQuads[q].m_aPoints[0].y));
				vec2 p1 = Position + vec2(fx2f(pQuads[q].m_aPoints[1].x), fx2f(pQuads[q].m_aPoints[1].y));
				vec2 p2 = Position + vec2(fx2f(pQuads[q].m_aPoints[2].x), fx2f(pQuads[q].m_aPoints[2].y));
				vec2 p3 = Position + vec2(fx2f(pQuads[q].m_aPoints[3].x), fx2f(pQuads[q].m_aPoints[3].y));

				if(Angle != 0)
				{
					vec2 center(fx2f(pQuads[q].m_aPoints[4].x), fx2f(pQuads[q].m_aPoints[4].y));
					Rotate(&center, &p0, Angle);
					Rotate(&center, &p1, Angle);
					Rotate(&center, &p2, Angle);
					Rotate(&center, &p3, Angle);
				}

				if(OutOfRange(Pos.x, p0.x, p1.x, p2.x, p3.x))
					continue;
				if(OutOfRange(Pos.y, p0.y, p1.y, p2.y, p3.y))
					continue;

				if(InsideQuad(p0, p1, p2, p3, Pos))
				{
					Index = pQuads[q].m_ColorEnvOffset;
					ExtraData = pQuads[q].m_aColors[0].g;
					break;
				}
			}
		}
	}

	if(pData)
	{
		pData->Index = Index;
		pData->ExtraData = ExtraData;
	}

	return Index;
}
int CCollision::GetZoneValueAt2(int ZoneHandle, vec2 Pos, double time, ZoneData *pData)
{
	if(!m_pLayers->ZoneGroup())
		return 0;

	if(ZoneHandle < 0 || ZoneHandle >= m_Zones.size())
		return 0;

	int Index = 0;
	int ExtraData = 0;

	SAnimationTransformCache AnimationCache;

	const array<CMapItemLayer *> &Zone = m_Zones[ZoneHandle];
	for(int i = 0; i < Zone.size(); i++)
	{
		const CMapItemLayer *pLayer = Zone[i];
		if(pLayer->m_Type == LAYERTYPE_TILES)
		{
			const CMapItemLayerTilemap *pTLayer = (const CMapItemLayerTilemap *)pLayer;
			const CTile *pTiles = (const CTile *)m_pLayers->Map()->GetData(pTLayer->m_Data);

			int Nx = std::clamp(static_cast<int>(Pos.x) / 32, 0, pTLayer->m_Width - 1);
			int Ny = std::clamp(static_cast<int>(Pos.y) / 32, 0, pTLayer->m_Height - 1);

			int TileIndex = (pTiles[Ny * pTLayer->m_Width + Nx].m_Index > 128 ? 0 : pTiles[Ny * pTLayer->m_Width + Nx].m_Index);
			if(TileIndex > 0)
			{
				Index = TileIndex;
				break;
			}
		}
		else if(pLayer->m_Type == LAYERTYPE_QUADS)
		{
			const CMapItemLayerQuads *pQLayer = (const CMapItemLayerQuads *)pLayer;
			const CQuad *pQuads = (const CQuad *)m_pLayers->Map()->GetDataSwapped(pQLayer->m_Data);

			for(int q = 0; q < pQLayer->m_NumQuads; q++)
			{
				vec2 Position(0.0f, 0.0f);
				float Angle = 0.0f;
				if(pQuads[q].m_PosEnv >= 0)
				{
					if(pQuads[q].m_PosEnv != AnimationCache.PosEnv)
					{
						AnimationCache.PosEnv = pQuads[q].m_PosEnv;
						GetAnimationTransform(time, AnimationCache.PosEnv, m_pLayers, AnimationCache.Position, AnimationCache.Angle);
					}

					Position = AnimationCache.Position;
					Angle = AnimationCache.Angle;
				}

				vec2 p0 = Position + vec2(fx2f(pQuads[q].m_aPoints[0].x), fx2f(pQuads[q].m_aPoints[0].y));
				vec2 p1 = Position + vec2(fx2f(pQuads[q].m_aPoints[1].x), fx2f(pQuads[q].m_aPoints[1].y));
				vec2 p2 = Position + vec2(fx2f(pQuads[q].m_aPoints[2].x), fx2f(pQuads[q].m_aPoints[2].y));
				vec2 p3 = Position + vec2(fx2f(pQuads[q].m_aPoints[3].x), fx2f(pQuads[q].m_aPoints[3].y));

				if(Angle != 0)
				{
					vec2 center(fx2f(pQuads[q].m_aPoints[4].x), fx2f(pQuads[q].m_aPoints[4].y));
					Rotate(&center, &p0, Angle);
					Rotate(&center, &p1, Angle);
					Rotate(&center, &p2, Angle);
					Rotate(&center, &p3, Angle);
				}

				if(OutOfRange(Pos.x, p0.x, p1.x, p2.x, p3.x))
					continue;
				if(OutOfRange(Pos.y, p0.y, p1.y, p2.y, p3.y))
					continue;

				if(InsideQuad(p0, p1, p2, p3, Pos))
				{
					Index = pQuads[q].m_PosEnvOffset;
					ExtraData = pQuads[q].m_aColors[0].g;
					break;
				}
			}
		}
	}

	if(pData)
	{
		pData->Index = Index;
		pData->ExtraData = ExtraData;
	}

	return Index;
}

// ��������������������ϵ�ͶӰ����
void GetProjection(const vec2 *Points, int Count, vec2 Axis, float &Min, float &Max)
{
	Min = Max = dot(Points[0], Axis);
	for(int i = 1; i < Count; i++)
	{
		float Projection = dot(Points[i], Axis);
		if(Projection < Min)
			Min = Projection;
		if(Projection > Max)
			Max = Projection;
	}
}

int CCollision::GetZoneValueRect(int ZoneHandle, vec2 Pos, vec2 Size, double time, ZoneData *pData)
{
	if(!m_pLayers->ZoneGroup() || ZoneHandle < 0 || ZoneHandle >= (int)m_Zones.size())
		return 0;

	int Index = 0;
	int ExtraData = 0;

	// 1. ׼����Ҿ��ε� 4 ������ (AABB ����)
	float R_X = Size.x / 2.0f;
	float R_Y = Size.y / 2.0f;
	vec2 RectPoints[4] = {
		Pos + vec2(-R_X, -R_Y), Pos + vec2(R_X - 2, -R_Y),
		Pos + vec2(R_X, R_Y), Pos + vec2(-R_X - 2, R_Y)};

	SAnimationTransformCache AnimationCache;
	const array<CMapItemLayer *> &Zone = m_Zones[ZoneHandle];

	for(int i = 0; i < Zone.size(); i++)
	{
		const CMapItemLayer *pLayer = Zone[i];

		// --- Tile ���� ---
		if(pLayer->m_Type == LAYERTYPE_TILES)
		{
			const CMapItemLayerTilemap *pTLayer = (const CMapItemLayerTilemap *)pLayer;
			const CTile *pTiles = (const CTile *)m_pLayers->Map()->GetData(pTLayer->m_Data);

			int StartX = std::clamp(static_cast<int>(Pos.x - R_X) / 32, 0, pTLayer->m_Width - 1);
			int EndX = std::clamp(static_cast<int>(Pos.x + R_X) / 32, 0, pTLayer->m_Width - 1);
			int StartY = std::clamp(static_cast<int>(Pos.y - R_Y) / 32, 0, pTLayer->m_Height - 1);
			int EndY = std::clamp(static_cast<int>(Pos.y + R_Y) / 32, 0, pTLayer->m_Height - 1);

			for(int y = StartY; y <= EndY; y++)
				for(int x = StartX; x <= EndX; x++)
				{
					int TileIndex = pTiles[y * pTLayer->m_Width + x].m_Index;
					if(TileIndex > 0 && TileIndex <= 128)
					{
						Index = TileIndex;
						goto EndDetection;
					}
				}
		}
		// --- Quad ���� (SAT �㷨) ---
		else if(pLayer->m_Type == LAYERTYPE_QUADS)
		{
			const CMapItemLayerQuads *pQLayer = (const CMapItemLayerQuads *)pLayer;
			const CQuad *pQuads = (const CQuad *)m_pLayers->Map()->GetDataSwapped(pQLayer->m_Data);

			for(int q = 0; q < pQLayer->m_NumQuads; q++)
			{
				vec2 Position(0.0f, 0.0f);
				float Angle = 0.0f;
				if(pQuads[q].m_PosEnv >= 0)
				{
					if(pQuads[q].m_PosEnv != AnimationCache.PosEnv)
					{
						AnimationCache.PosEnv = pQuads[q].m_PosEnv;
						GetAnimationTransform(time, AnimationCache.PosEnv, m_pLayers, AnimationCache.Position, AnimationCache.Angle);
					}
					Position = AnimationCache.Position;
					Angle = AnimationCache.Angle;
				}

				// ���� Quad ���㲢Ӧ����ת
				vec2 QuadPoints[4];
				for(int j = 0; j < 4; j++)
					QuadPoints[j] = Position + vec2(fx2f(pQuads[q].m_aPoints[j].x), fx2f(pQuads[q].m_aPoints[j].y));

				if(Angle != 0)
				{
					vec2 center(fx2f(pQuads[q].m_aPoints[4].x), fx2f(pQuads[q].m_aPoints[4].y));
					for(int j = 0; j < 4; j++)
						Rotate(&center, &QuadPoints[j], Angle);
				}

				// �����ᶨ�� (SAT) �ж�
				bool Overlap = true;
				vec2 Axes[6];
				Axes[0] = vec2(1, 0); // ��Ҿ����� X
				Axes[1] = vec2(0, 1); // ��Ҿ����� Y
				for(int j = 0; j < 4; j++)
				{
					vec2 Edge = QuadPoints[(j + 1) % 4] - QuadPoints[j];
					Axes[j + 2] = normalize(vec2(-Edge.y, Edge.x)); // Quad �ı߷���
				}

				for(int a = 0; a < 6; a++)
				{
					float MinR, MaxR, MinQ, MaxQ;
					GetProjection(RectPoints, 4, Axes[a], MinR, MaxR);
					GetProjection(QuadPoints, 4, Axes[a], MinQ, MaxQ);

					if(MaxR < MinQ || MaxQ < MinR)
					{ // ֻҪ��һ���᲻�ص�����û����
						Overlap = false;
						break;
					}
				}

				if(Overlap)
				{
					Index = pQuads[q].m_ColorEnvOffset;
					ExtraData = pQuads[q].m_aColors[0].g;
					goto EndDetection;
				}
			}
		}
	}

EndDetection:
	if(pData)
	{
		pData->Index = Index;
		pData->ExtraData = ExtraData;
	}
	return Index;
}
CQuad CCollision::GetZoneValueRectPos(int ZoneHandle, vec2 Pos, vec2 Size, double time, ZoneData *pData)
{
	// 1. 初始化一个默认返回值（用于未发现碰撞或参数无效的情况）
	CQuad Index = {};
	// int ExtraData = 0;

	// 修复：原先这里是空返回 return;，必须返回 Index
	if(!m_pLayers->ZoneGroup() || ZoneHandle < 0 || ZoneHandle >= (int)m_Zones.size())
		return Index;

	// 准备矩形的 4 个顶点 (AABB 逻辑)
	float R_X = Size.x / 2.0f;
	float R_Y = Size.y / 2.0f;
	vec2 RectPoints[4] = {
		Pos + vec2(-R_X, -R_Y), Pos + vec2(R_X - 2, -R_Y),
		Pos + vec2(R_X, R_Y), Pos + vec2(-R_X - 2, R_Y)};

	SAnimationTransformCache AnimationCache;
	const array<CMapItemLayer *> &Zone = m_Zones[ZoneHandle];

	for(int i = 0; i < Zone.size(); i++)
	{
		const CMapItemLayer *pLayer = Zone[i];

		// --- Quad 层检测 (SAT 算法) ---
		if(pLayer->m_Type == LAYERTYPE_QUADS)
		{
			const CMapItemLayerQuads *pQLayer = (const CMapItemLayerQuads *)pLayer;
			const CQuad *pQuads = (const CQuad *)m_pLayers->Map()->GetDataSwapped(pQLayer->m_Data);

			for(int q = 0; q < pQLayer->m_NumQuads; q++)
			{
				vec2 Position(0.0f, 0.0f);
				float Angle = 0.0f;
				if(pQuads[q].m_PosEnv >= 0)
				{
					if(pQuads[q].m_PosEnv != AnimationCache.PosEnv)
					{
						AnimationCache.PosEnv = pQuads[q].m_PosEnv;
						GetAnimationTransform(time, AnimationCache.PosEnv, m_pLayers, AnimationCache.Position, AnimationCache.Angle);
					}
					Position = AnimationCache.Position;
					Angle = AnimationCache.Angle;
				}

				// 计算 Quad 顶点并应用旋转
				vec2 QuadPoints[4];
				for(int j = 0; j < 4; j++)
					QuadPoints[j] = Position + vec2(fx2f(pQuads[q].m_aPoints[j].x), fx2f(pQuads[q].m_aPoints[j].y));

				if(Angle != 0)
				{
					vec2 center(fx2f(pQuads[q].m_aPoints[4].x), fx2f(pQuads[q].m_aPoints[4].y));
					for(int j = 0; j < 4; j++)
						Rotate(&center, &QuadPoints[j], Angle);
				}

				// 分离轴定理 (SAT) 判断
				bool Overlap = true;
				vec2 Axes[6];
				Axes[0] = vec2(1, 0); // 矩形本地轴 X
				Axes[1] = vec2(0, 1); // 矩形本地轴 Y
				for(int j = 0; j < 4; j++)
				{
					vec2 Edge = QuadPoints[(j + 1) % 4] - QuadPoints[j];
					Axes[j + 2] = normalize(vec2(-Edge.y, Edge.x)); // Quad 的边法线
				}

				for(int a = 0; a < 6; a++)
				{
					float MinR, MaxR, MinQ, MaxQ;
					GetProjection(RectPoints, 4, Axes[a], MinR, MaxR);
					GetProjection(QuadPoints, 4, Axes[a], MinQ, MaxQ);

					if(MaxR < MinQ || MaxQ < MinR)
					{
						Overlap = false;
						break;
					}
				}

				if(Overlap)
				{
					Index = pQuads[q];
					// ExtraData = pQuads[q].m_aColors[0].g;
					// 如果只需要检测到第一个碰撞就返回，可以在这里直接 return Index;
				}
			}
		}
	}

	return Index;
}
