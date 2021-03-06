#include "c4/tpl/token.hpp"

namespace c4 {
namespace tpl {

C4_DEFINE_MANAGED_BASE(TokenBase, size_t);
C4_DEFINE_MANAGED(TokenExpression, size_t);
C4_DEFINE_MANAGED(TokenIf, size_t);
C4_DEFINE_MANAGED(TokenFor, size_t);
C4_DEFINE_MANAGED(TokenComment, size_t);


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void TokenBase::parse(csubstr *rem, TplLocation *curr_pos)
{
    auto const s = stoken(), e = etoken();
    C4_ASSERT(rem->begins_with(s));

    m_start = *curr_pos;
    // look for the end token, but skip nested start/end token pairs
    csubstr rem2 = skip_nested(*rem);
    m_full_text = rem->left_of(rem2);
    C4_ASSERT(m_full_text.len >= e.len + s.len);
    C4_ASSERT(m_full_text.begins_with(s));
    C4_ASSERT(m_full_text.ends_with(e));
    m_interior_text = m_full_text.sub(s.len, m_full_text.len - (e.len + s.len));

    // if the end token terminates with a line ending, "merge" it with any
    // line ending of the interior text
    *rem = rem->sub(m_full_text.len);
    if(m_interior_text.ends_with_any("\r\n") && rem->begins_with_any("\r\n"))
    {
        size_t inc = 0;
        if((*rem)[0] == '\r' && (*rem)[1] == '\n') inc = 2;
        else if((*rem)[0] == '\n') inc = 1;
        m_full_text.len += inc;
        *rem = rem->sub(inc);
    }

    auto &rp = curr_pos->m_rope_pos;
    C4_ASSERT(curr_pos->m_rope->get(rp.entry)->s.len >= m_full_text.len);
    rp.entry = curr_pos->m_rope->replace(rp.entry, rp.i, m_full_text.len, m_full_text);
    m_rope_entry = rp.entry;
    rp.entry = curr_pos->m_rope->next(rp.entry);
    rp.i = 0;
    m_end = *curr_pos;
    C4_ASSERT(this->sub() == m_full_text);
}

void TokenBase::mark()
{
    m_start.m_rope->replace(m_start.m_rope_pos.entry, marker());
}

TokenBase::PropResult TokenBase::get_property(NodeRef const& root, csubstr key, bool inside_brackets)
{
    NodeRef n = root;

    // if it is quoted, this is our value
    if(key.begins_with('\'') || key.ends_with('"'))
    {
        PropResult pr;
        pr.n = NodeRef();
        pr.val = key.unquoted();
        pr.success = true;
        return pr;
    }

    do {
        auto pos = key.find('.');
        if(pos != npos)
        {
            csubstr left = key.left_of(pos);
            n = n.find_child(left);
            key = key.right_of(pos);
        }
        else
        {
            pos = key.find('[');
            if(pos != npos)
            {
                csubstr left = key.left_of(pos);
                if( ! left.empty())
                {
                    n = n.find_child(left);
                    if( ! n.valid())
                    {
                        break;
                    }
                    key = key.right_of(pos);
                    pos = key.find(']');
                    C4_ASSERT(pos != npos);
                    csubstr subkey = key.left_of(pos);
                    key = key.right_of(pos);
                    return get_property(n, subkey, true);
                }
            }
            else
            {
                if(key.begins_with_any("0123456789"))
                {
                    if(inside_brackets) // assume it's an integer indexing into this node's children
                    {
                        size_t num;
                        bool ret = from_chars(key, &num);
                        if(ret)
                        {
                            if(n.num_children() >= num)
                            {
                                n = n[num];
                                key.clear();
                            }
                        }
                    }
                    else // it's not inside brackets, so assume it's a literal
                    {
                        PropResult pr;
                        pr.val = key;
                        pr.success = true;
                        return pr;
                    }
                }
                else
                {
                    n = n.find_child(key);
                    key.clear();
                }
            }
        }
    } while( ! key.empty() && n.valid());

    PropResult pr;
    pr.n = n;
    pr.val.clear();
    pr.success = n.valid();

    return pr;
}

bool TokenBase::eval(NodeRef const& root, csubstr key, csubstr *value) const
{
    C4_ASSERT(root.valid());
    PropResult pr = get_property(root, key);

    if(pr)
    {
        if(pr.n.valid())
        {
            if(pr.n.is_map()) *value = "<<<map>>>";
            else if(pr.n.is_seq()) *value = "<<<seq>>>";
            else *value = pr.n.val();
            return true;
        }
        else
        {
            *value = pr.val;
            return true;
        }
    }

    return false;
}

csubstr TokenBase::skip_nested(csubstr rem) const
{
    auto const& s = stoken(), e = etoken();
    C4_ASSERT(rem.begins_with(s));
    csubstr r = rem.sub(s.len);
    size_t level = 1;
    while( ! r.empty())
    {
        auto result = r.first_of_any(s, e);
        C4_CHECK_MSG(result, "invalid nested sequence");
        C4_CHECK_MSG(result.which == 0 || result.which == 1, "internal error");
        if(result.which == 0)
        {
            ++level;
        }
        else
        {
            C4_CHECK_MSG(level > 0, "internal error");
            --level;
        }
        r = r.sub(result.pos + (result.which == 0 ? s.len : e.len));
        if(level == 0)
        {
            return r;
        }
    }
    C4_ERROR("never reach this");
    return {};
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void TemplateBlock::parse(TokenContainer *cont)
{
    // this object may be relocated while this function is executed.
    size_t tkid = owner_id, bid = block_id;
#define _c4this cont->get(tkid)->get_block(bid)

    _c4this->parts.clear();
    _c4this->tokens = cont;
    C4_ASSERT(_c4this->start.m_rope != nullptr);
    csubstr curr = _c4this->body;
    csubstr rem = _c4this->body;
    TplLocation pos = _c4this->start;
    Rope *rope = pos.m_rope;
    auto &rp = pos.m_rope_pos;
    rope->replace(rp.entry, _c4this->body);
    rp.i = 0;
    while( ! rem.empty())
    {
        C4_ASSERT(rem == rope->sub(rp).sub(0, rem.len));
        size_t tk_pos = cont->next_token(&rem, &pos);
        if(tk_pos == NONE)
        {
            _c4this->parts.emplace_back();
            auto &p = _c4this->parts.back();
            p.body = rem;
            p.entry = rope->replace(rp.entry, rp.i, p.body.len, p.body);
            curr = rem;
            break;
        }
        // is there an entry for the block before the token?
        if(rem.begin() != curr.begin())
        {
            _c4this->parts.emplace_back();
            auto &p = _c4this->parts.back();
            p.body = curr.sub(0, rem.begin() - curr.begin());
            p.entry = rp.entry;
            rp.entry = rope->split(rp.entry, p.body.len);
            rp.entry = rope->next(rp.entry);
            C4_ASSERT(rp.entry != NONE);
            rp.i = 0;
            curr = rem;
        }
        // add the entry for the token
        {
            _c4this->parts.emplace_back();
            auto &p = _c4this->parts.back();
            p.token = tk_pos;
            p.entry = NONE;
            p.body = rem;
            rope->replace(rp.entry, rem);
            cont->get(tk_pos)->m_root_level = false;
            cont->get(tk_pos)->parse(&rem, &pos);
            cont->get(tk_pos)->parse_body(cont);
            C4_ASSERT(p.body.contains(rem));
            p.body = p.body.sub(0, rem.begin() - p.body.begin());
            curr = rem;
        }
    }

#undef _c4this
}

size_t TemplateBlock::render(NodeRef & root, Rope *rope) const
{
    size_t e = NONE;
    for(auto const& p : parts)
    {
        if(p.token != NONE)
        {
            e = tokens->get(p.token)->render(root, rope);
        }
        else
        {
            rope->replace(p.entry, p.body);
            e = p.entry;
        }
    }
    return e;
}

size_t TemplateBlock::duplicate(NodeRef & root, Rope *rope, size_t start_entry) const
{
    for(auto const& p : parts)
    {
        if(p.token != NONE)
        {
            start_entry = tokens->get(p.token)->duplicate(root, rope, start_entry);
        }
        else
        {
            start_entry = rope->insert_after(start_entry, p.body);
        }
    }
    return start_entry;
}

void TemplateBlock::clear(Rope *rope) const
{
    for(auto const& p : parts)
    {
        if(p.token != NONE)
        {
            tokens->get(p.token)->clear(rope);
        }
        else
        {
            rope->replace(p.entry, {});
        }
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void IfCondition::_eval(TokenIf const* tk, NodeRef & root)
{
    m_argval = {};
    if( ! m_arg.empty())
    {
        tk->eval(root, m_arg, &m_argval);
    }
    m_cmpval = {};
    if( ! m_cmp.empty())
    {
        tk->eval(root, m_cmp, &m_cmpval);
    }
}

bool IfCondition::resolve(TokenIf const* tk, NodeRef & root)
{
    switch(m_ctype)
    {
    case ELSE:                        return   true;
    case ARG:        _eval(tk, root); return ! m_argval.empty();
    case ARG_EQ_CMP: _eval(tk, root); return   m_argval.compare(m_cmpval) == 0;
    case ARG_NE_CMP: _eval(tk, root); return   m_argval.compare(m_cmpval) != 0;
    case ARG_GE_CMP: _eval(tk, root); return   m_argval.compare(m_cmpval) >= 0;
    case ARG_GT_CMP: _eval(tk, root); return   m_argval.compare(m_cmpval) >  0;
    case ARG_LE_CMP: _eval(tk, root); return   m_argval.compare(m_cmpval) <= 0;
    case ARG_LT_CMP: _eval(tk, root); return   m_argval.compare(m_cmpval) <  0;
    case ARG_IN_CMP:
    case ARG_NOT_IN_CMP:
    {
        bool in_cmp;
        C4_ASSERT(root.is_map());
        NodeRef n = root.find_child(m_cmp);
        if(n.valid())
        {
            if(n.is_map())
            {
                in_cmp = n.find_child(m_arg).valid();
            }
            else if(n.is_seq())
            {
                in_cmp = false;
                for(auto ch : n.children())
                {
                    if(ch.is_val() && ch.val() == m_arg)
                    {
                        in_cmp = true;
                        break;
                    }
                }
            }
            else
            {
                return false;
            }
            return m_ctype == ARG_IN_CMP ? in_cmp : ! in_cmp;
        }
        return false;
    }
    // do the other condition types
    default:
        C4_ERROR("unknown condition type");
        break;
    }
    C4_ERROR("never reach");
    return false;
}

void IfCondition::parse()
{
    /** @todo the scanning is inefficient. Use a for loop to iterate
     * through the string characters instead of calling s.find(). This 
     * was written in a hurry, which explains the lazy use of find(). */

    C4_ASSERT(m_str.first_of("{}*") == npos);
    auto pos = m_str.first_of('<');
    if(pos != npos)
    {
        C4_ASSERT(pos+2 < m_str.len);
        if(m_str[pos+1] == '=')
        {
            m_ctype = ARG_LE_CMP;
            m_arg = m_str.left_of(pos).trim(' ');
            m_cmp = m_str.right_of(pos+2, /*include*/true).trim(' ');
        }
        else
        {
            m_ctype = ARG_LT_CMP;
            m_arg = m_str.left_of(pos).trim(' ');
            m_cmp = m_str.right_of(pos+1, /*include*/true).trim(' ');
        }
        return;
    }

    pos = m_str.first_of('>');
    if(pos != npos)
    {
        C4_ASSERT(pos+2 < m_str.len);
        if(m_str[pos+1] == '=')
        {
            m_ctype = ARG_GE_CMP;
            m_arg = m_str.left_of(pos).trim(' ');
            m_cmp = m_str.right_of(pos+2, /*include*/true).trim(' ');
        }
        else
        {
            m_ctype = ARG_GT_CMP;
            m_arg = m_str.left_of(pos).trim(' ');
            m_cmp = m_str.right_of(pos+1, /*include*/true).trim(' ');
        }
        return;
    }

    pos = m_str.first_of('!');
    if(pos != npos) // !=
    {
        C4_ASSERT(pos+2 < m_str.len);
        C4_ASSERT(m_str[pos+1] == '=');
        m_ctype = ARG_NE_CMP;
        m_arg = m_str.left_of(pos).trim(' ');
        m_cmp = m_str.right_of(pos+2, /*include*/true).trim(' ');
        return;
    }

    pos = m_str.first_of('=');
    if(pos != npos) // ==
    {
        C4_ASSERT(pos+2 < m_str.len);
        C4_ASSERT(m_str[pos+1] == '=');
        m_ctype = ARG_EQ_CMP;
        m_arg = m_str.left_of(pos).trim(' ');
        m_cmp = m_str.right_of(pos+2, /*include*/true).trim(' ');
        return;
    }

    pos = m_str.find(" not in ");
    if(pos != npos) //
    {
        m_ctype = ARG_NOT_IN_CMP;
        m_arg = m_str.left_of(pos).trim(' ');
        m_cmp = m_str.right_of(pos+8, /*include*/true).trim(' ');
        return;
    }

    pos = m_str.find(" in ");
    if(pos != npos) //
    {
        m_ctype = ARG_IN_CMP;
        m_arg = m_str.left_of(pos).trim(' ');
        m_cmp = m_str.right_of(pos+4, /*include*/true).trim(' ');
        return;
    }

    m_ctype = ARG;
    m_arg = m_str.trim(' ');
    m_cmp.clear();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void TokenIf::parse(csubstr *rem, TplLocation *curr_pos)
{
    base_type::parse(rem, curr_pos);

    // scan the condition
    csubstr s = m_full_text;
    csubstr c = _scan_condition(stoken(), &s);
    condblock *cb = _add_block(c, s);
    size_t block_beginning = s.str - m_full_text.str;
    size_t block_size = 0;

    // scan the branches
    while( ! s.empty())
    {
        auto result = s.first_of_any("{% endif %}", "{% else %}", "{% elif ", "{% if ");
        C4_CHECK_MSG(result, "invalid {% if %} structure");
        if(result.which == 0) // endif
        {
            block_size += result.pos;
            csubstr block_body = m_full_text.sub(block_beginning, block_size);
            cb->set_body(block_body); // terminate the current block
            break;
        }
        else if(result.which == 1) // else
        {
            // finish the current block
            block_size += result.pos;
            csubstr block_body = m_full_text.sub(block_beginning, block_size);
            cb->set_body(block_body);
            // consume the else block
            block_beginning += block_size + 10;  // 10==strlen("{% else %}")
            block_size = 0;
            block_body = m_full_text.sub(block_beginning, block_size);
            cb = _add_block({}, block_body, /*as_else*/true);
            s = m_full_text.sub(block_beginning);
        }
        else if(result.which == 2) // elif
        {
            block_size += result.pos;
            csubstr block_body = m_full_text.sub(block_beginning, block_size);
            cb->set_body(block_body);
            s = s.sub(result.pos);
            csubstr cond = _scan_condition("{% elif ", &s);
            block_beginning = s.str - m_full_text.str;
            block_size = 0;
            cb = _add_block(cond, m_full_text.sub(block_beginning, block_size));
        }
        else if(result.which == 3) // nested if
        {
            csubstr r = skip_nested(s.sub(result.pos));
            block_size += (r.str - s.str);
            s = r;
        }
        else
        {
            C4_ERROR("internal error");
        }
    }

    for(auto &cond : m_blocks)
    {
        C4_ASSERT(m_full_text.contains(cond.body) || cond.body.empty());
        cond.body = cond.body.triml("\r\n");
        cond.start.m_rope_pos.i = cond.body.begin() - m_full_text.begin();
    }
}

TokenIf::condblock* TokenIf::_add_block(csubstr cond, csubstr s, bool as_else)
{
    C4_ASSERT(m_full_text.contains(s));
    m_blocks.emplace_back();
    auto *cb = &m_blocks.back();
    cb->body = s;
    cb->start.m_rope = m_start.m_rope;
    cb->start.m_rope_pos.i = 0;
    cb->owner_id = this->id();
    cb->block_id = m_blocks.size() - 1;
    if(m_blocks.size() == 1)
    {
        cb->start.m_rope_pos.entry = m_rope_entry;
    }
    else
    {
        C4_ASSERT(m_blocks.size() >= 2);
        auto prev = m_blocks[m_blocks.size() - 2].start.m_rope_pos.entry;
        cb->start.m_rope_pos.entry = m_start.m_rope->insert_after(prev, s);
    }
    if(as_else)
    {
        cb->condition.init_as_else();
    }
    else
    {
        cb->condition.init(cond);
    }
    return cb;
}

void TokenIf::parse_body(TokenContainer *cont) const
{
    // defend against relocation
#define _c4this static_cast< TokenIf const* >(cont->get(my_id))

    size_t my_id = this->id();
    for(auto &b : _c4this->m_blocks)
    {
        b.parse(cont);
    }

#undef _c4this
}

csubstr TokenIf::_scan_condition(csubstr token, csubstr *s)
{
    C4_ASSERT(s->begins_with(token));
    auto pos = s->find("%}"); // this is where the {% if ... %} tag ends
    C4_ASSERT(pos != npos);
    csubstr c = s->range(token.len, pos);
    c = c.trim(' ');
    *s = s->sub(pos + 2);
    return c;
}

bool TokenIf::resolve(NodeRef const& /*root*/, csubstr * /*value*/) const
{
    C4_ERROR("never call this");
    return true;
}

size_t TokenIf::render(NodeRef & root, Rope *rope) const
{
    TemplateBlock const* true_block = nullptr;

    // find the block corresponding to a true condition
    for(auto const& cb : m_blocks)
    {
        if(cb.condition.resolve(this, root))
        {
            true_block = &cb;
            break;
        }
    }

    // render that block (if it exists) and clear all other blocks
    size_t entry = NONE;
    for(auto const& cb : m_blocks)
    {
        if(&cb == true_block)
        {
            entry = cb.render(root, rope);
        }
        else
        {
            cb.clear(rope);
        }
    }

    if(entry == NONE)
    {
        entry = m_rope_entry;
    }

    return entry;
}

size_t TokenIf::duplicate(NodeRef & root, Rope *rope, size_t start_entry) const
{
    TemplateBlock const* true_block = nullptr;

    // find the block corresponding to a true condition
    for(auto const& cb : m_blocks)
    {
        if(cb.condition.resolve(this, root))
        {
            true_block = &cb;
            break;
        }
    }

    // duplicate that block (if it exists) and clear all other blocks
    for(auto const& cb : m_blocks)
    {
        if(&cb == true_block)
        {
            start_entry = cb.duplicate(root, rope, start_entry);
        }
        else
        {
            cb.clear(rope);
        }
    }

    return start_entry;
}

void TokenIf::clear(Rope *rope) const
{
    for(auto const& cb : m_blocks)
    {
        cb.clear(rope);
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool TokenFor::resolve(NodeRef const& /*root*/, csubstr * /*value*/) const
{
    C4_ERROR("never call this");
    return true;
}

void TokenFor::parse(csubstr *rem, TplLocation *curr_pos)
{
    base_type::parse(rem, curr_pos);

    csubstr s = m_interior_text;

    size_t pos = s.find(" in ");
    C4_CHECK_MSG(pos != npos, "parse error");
    m_var = s.left_of(pos);
    m_val = s.right_of(pos + 4, /*include_pos*/true);
    pos = m_val.first_of(' ');
    C4_CHECK_MSG(pos != npos, "parse error");
    m_val = m_val.left_of(pos);

    csubstr body = s.right_of(m_val);
    pos = body.find(" %}");
    C4_CHECK_MSG(pos != npos, "parse error");
    C4_CHECK_MSG(pos == 0, "parse error");
    body = body.right_of(2);
    body = body.triml("\r\n");

    m_block.owner_id = this->id();
    m_block.block_id = 0;
    m_block.body = body;
    m_block.start.m_rope = m_start.m_rope;
    m_block.start.m_rope_pos.i = body.begin() - m_full_text.begin();
    m_block.start.m_rope_pos.entry = m_rope_entry;
}

void TokenFor::parse_body(TokenContainer *cont) const
{
    // watchout for relocations!!!
    m_block.parse(cont);
}

size_t TokenFor::render(NodeRef & root, Rope * rope) const
{
    return _do_render(root, rope, NONE, false);
}

size_t TokenFor::duplicate(NodeRef & root, Rope *rope, size_t start_entry) const
{
    return _do_render(root, rope, start_entry, true);
}

void TokenFor::clear(Rope *rope) const
{
    m_block.clear(rope);
}

size_t TokenFor::_do_render(NodeRef& root, Rope *rope, size_t start_entry, bool duplicating) const
{
    PropResult pr = get_property(root, m_val);
    if(pr)
    {
        C4_ASSERT(pr.n.valid());
        bool first_child = true;
        size_t num = pr.n.num_children();
        size_t i = 0;
        for(auto ch : pr.n.children())
        {
            _set_loop_properties(root, ch, i++, num);
            if(first_child && !duplicating)
            {
                start_entry = m_block.render(root, rope);
            }
            else
            {
                if(start_entry == NONE) start_entry = m_rope_entry;
                start_entry = m_block.duplicate(root, rope, start_entry);
            }
            first_child = false;
            _clear_loop_properties(root);
        }
    }

    if(start_entry == NONE)
    {
        start_entry = m_rope_entry;
        m_block.clear(rope);
    }

    return start_entry;
}

void TokenFor::_set_loop_properties(NodeRef & root, NodeRef const& var, size_t i, size_t num) const
{
    C4_ASSERT(num > 0);
    C4_CHECK_MSG(!root.find_child(m_var).valid(), "cannot use an existing name for the loop variable value");
    C4_CHECK_MSG(!root.find_child("loop").valid(), "cannot use an existing name for the loop information variable");
    C4_CHECK_MSG(root.is_map() || root.is_seq(), "for can only loop over containers");

    auto v = root.append_child();
    v.set_key(m_var);
    if(var.is_container())
    {
        if(var.has_key())
        {
            v |= yml::SEQ;
            v[0] = var.key();
            var.duplicate(v, v.last_child());
        }
        else
        {
            //var.duplicate(root, root.last_child());
            var.duplicate_children(v, v.last_child());
        }
        if(var.is_map()) v |= yml::MAP;
        else if(var.is_seq()) v |= yml::SEQ;
    }
    else
    {
        v = var.val();
    }

    auto l = root.append_child();
    l |= yml::MAP;
    l.set_key("loop");
    l["index"] << i;              // The current iteration of the loop. (0 indexed)
    l["length"] << num;           // The number of items in the sequence.
    l["revindex"] << num - i - 1; // The number of iterations from the end of the loop (0 indexed)
    l["first"] << (i == 0);       // "1" if first iteration, "0" otherwise.
    l["last"] << (i == num-1);    // "1" if last iteration, "0" otherwise.
    l["odd"] = to_csubstr((i & 1) != 0 ? "1" : "0");
    l["even"] = to_csubstr((i & 1) == 0 ? "1" : "0");
}

void TokenFor::_clear_loop_properties(NodeRef & root) const
{
    root.remove_child(m_var);
    root.remove_child("loop");
}

} // namespace tpl
} // namespace c4
