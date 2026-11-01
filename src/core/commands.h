#ifndef CORE_COMMANDS_H
#define CORE_COMMANDS_H
inline constexpr int ClientInputMessageTypeMove       = 0x2;
inline constexpr int ClientInputMessagePredictionFlag = 1;
void                 client_input_resolve_all();
int                  enqueue_move_input(int x, int y);
int                  client_prediction_move_to(int x, int y);
void                 commands_flush();
#endif
