#ifndef CORE_COMMANDS_H
#define CORE_COMMANDS_H
inline constexpr int ClientInputMessageTypeMove       = 0x2;
inline constexpr int ClientInputMessagePredictionFlag = 1;
inline constexpr int ClientInputMessageTypeMax        = 0x17;
int                  enqueue_move_input(int x, int y);
int                  enqueue_move_input_with_type(int x, int y, int type);
int                  client_prediction_move_to(int x, int y);
void                 commands_flush();
#endif
