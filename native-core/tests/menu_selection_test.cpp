#include "ggfm/menu_selection.hpp"
#include <cassert>
#include <cstdio>

int main() {
  ggfm::MenuSelectionQueue queue;
  assert(!queue.Queue(0, 0));
  const auto first = queue.Publish();
  assert(!queue.Queue(first, -1));
  assert(!queue.Queue(first, 4096));
  assert(queue.Queue(first, 0));
  assert(!queue.Queue(first, 1));
  const auto picked = queue.Consume();
  assert(picked && picked->index == 0 && queue.Current(*picked));
  assert(!queue.Consume());
  assert(queue.Queue(first, 1));
  const auto next = queue.Publish();
  const auto stale = queue.Consume();
  assert(stale && !queue.Current(*stale));
  assert(!queue.Queue(first, 0));
  assert(queue.Queue(next, 4095));
  const auto last = queue.Consume();
  assert(last && last->index == 4095 && queue.Current(*last));
  std::puts("menu selection queue: PASS");
  ggfm::MenuAmountQueue amounts;
  const auto prompt = amounts.Publish();
  assert(!amounts.Queue(0, "1"));
  assert(!amounts.Queue(prompt, ""));
  assert(!amounts.Queue(prompt, "1\"}"));
  assert(!amounts.Queue(prompt, "-1"));
  assert(!amounts.Queue(prompt, std::string(65, '1')));
  assert(amounts.Queue(prompt, "1A+250k"));
  assert(!amounts.Queue(prompt, "2"));
  const auto input = amounts.Consume();
  assert(input && input->amount == "1A+250k" && amounts.Current(*input));
  assert(!amounts.Consume());
  const auto reopened = amounts.Publish();
  assert(!amounts.Current(*input));
  assert(!amounts.Queue(prompt, "1"));
  assert(amounts.Queue(reopened, "2.5"));
  amounts.Publish();
  assert(!amounts.Consume());
  std::puts("menu amount queue: PASS");
}
