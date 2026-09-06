#pragma once

void parser_reset(void);
void parser_feed_voice_text(const char *text);
const char *parser_expression(void);
