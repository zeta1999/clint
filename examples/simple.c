#pragma scop
for (int i = 0; i < N; i++)
  for (int j = 0; j < N; j++) {
    A[i][j] = 0;
  }
#pragma endscop
