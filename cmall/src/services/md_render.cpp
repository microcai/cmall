#include "stdafx.hpp"

#include <string>
#include <string_view>

#include "md_render.hpp"

#include "cmark.h"
#include "node.h"

static unsigned char * cmark_mem_strdup(std::string_view str, cmark_mem* mem)
{
	auto new_str = mem->calloc(str.length(), 1);
	std::memcpy(new_str, str.data(), str.length() + 1);
	return reinterpret_cast<unsigned char*>(new_str);
}

static void node_walker(mdrender::url_replacer replacer, cmark_node* node, cmark_event_type ev_type)
{
	auto cmark_mem_ = node->mem;

	if (node->type == CMARK_NODE_IMAGE && ev_type == CMARK_EVENT_EXIT)
	{
		unsigned char* old_url = node->as.link.url;

		boost::cmatch w;

		// 检查 url 尾部是不是有 =widthXheight 的.
		if (boost::regex_match(reinterpret_cast<char*>(old_url), w, boost::regex("([^ ]+) +=([0-9]+)x([0-9]+)")))
		{
			cmark_node* size_style = cmark_node_new_with_mem(CMARK_NODE_CUSTOM_INLINE, cmark_mem_);
			size_style->as.custom.on_enter = cmark_mem_strdup(std::format("\" style=\"width:{}px;height:{}px", w[2].str(), w[3].str()), cmark_mem_);
			// size_style->as.custom.on_enter = cmark_mem_->calloc()
			cmark_node_append_child(node, size_style);

			auto new_url = replacer(w[1].str());
			node->as.link.url = cmark_mem_strdup(new_url, cmark_mem_);// reinterpret_cast<unsigned char*>(cmark_mem_->calloc(new_url.length() + 1, 1));
			cmark_mem_->free(old_url);
		}
		else
		{
			auto new_url = replacer(std::string((char*)old_url));
			node->as.link.url = cmark_mem_strdup(new_url, cmark_mem_);// reinterpret_cast<unsigned char*>(cmark_mem_->calloc(new_url.length() + 1, 1));
			cmark_mem_->free(old_url);
		}
	}
}

std::string mdrender::markdown_to_html(std::string_view original, url_replacer replacer)
{
	std::shared_ptr<cmark_node> ast;

	ast.reset(
        cmark_parse_document(original.begin(), original.length(), CMARK_OPT_VALIDATE_UTF8 | CMARK_OPT_UNSAFE | CMARK_OPT_SMART),
		cmark_node_free
    );

	auto cmark_mem = ast->mem;

	cmark_event_type ev_type;
	cmark_node* cur;
	std::shared_ptr<cmark_iter> iter;
	iter.reset(cmark_iter_new(ast.get()), cmark_iter_free);

	while ((ev_type = cmark_iter_next(iter.get())) != CMARK_EVENT_DONE)
	{
		cur = cmark_iter_get_node(iter.get());
		node_walker(replacer, cur, ev_type);
	}

	auto html = cmark_render_html(ast.get(), CMARK_OPT_VALIDATE_UTF8 | CMARK_OPT_UNSAFE | CMARK_OPT_SMART);

	std::string ret;
    ret.assign(html);
	cmark_mem->free(html);

	return ret;
}
