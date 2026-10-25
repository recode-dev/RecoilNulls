#include "data.hpp"

__attribute__((constructor)) void start(void)
{
    dispatch_async(dispatch_get_main_queue(), ^{
      poll_for_game(0);
    });
}
