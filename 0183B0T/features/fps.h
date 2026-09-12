#pragma once

int* GetFpsLimitAddress();
int GetFpsLimit();

int GetDesiredFpsLimit();

void SetFpsLimit(int value);
void EnforceFpsLimit();
void ResetFpsLimit();