#include "ScarecrowEnemy.h"
#include "Engine/World.h"

AScarecrowEnemy::AScarecrowEnemy()
{
    // 기본적으로 AI 비활성화
    bCanAttack = false;
    bIsDead = false;
}

void AScarecrowEnemy::BeginPlay()
{
    Super::BeginPlay();

    // 체력 초기화
    CurrentHealth = MaxHealth;
}

void AScarecrowEnemy::StartChasing()
{
    // 허수아비는 추적하지 않음
}

void AScarecrowEnemy::Attack()
{
    // 허수아비는 공격하지 않음
}

void AScarecrowEnemy::ResetHealth()
{
    CurrentHealth = MaxHealth;
    UpdateHealthBar();
} 