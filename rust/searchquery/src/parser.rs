// SPDX-License-Identifier: GPL-3.0-or-later

//! Prototype of the advanced search query parser in `src/core/searchquery.cpp`.
//!
//! It validates queries and produces the same normalized form as
//! `SearchQuery::Query::toString()`. It does not evaluate queries against files,
//! localize messages or consult the MIME database; MIME types are checked for
//! syntax only.

use std::fmt;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Field {
    Name,
    Path,
    Type,
    Size,
    Modified,
    Content,
    Tag,
    Tagged,
    Metadata,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Comparison {
    Contains,
    Equal,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TypeMatch {
    File,
    Folder,
    MimeType,
    MimePrefix,
    Extension,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Term {
    pub field: Field,
    pub comparison: Comparison,
    /// The value as written, or the normalized type for type criteria.
    pub text: String,
    pub type_match: TypeMatch,
    /// Bytes, for size criteria.
    pub size: i64,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Node {
    Term(Term),
    And(Vec<Node>),
    Or(Vec<Node>),
    Not(Box<Node>),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Kind {
    Word,
    Open,
    Close,
    And,
    Or,
    Not,
}

#[derive(Clone, Debug)]
struct Token {
    kind: Kind,
    field: String,
    op: String,
    value: String,
    quoted: bool,
    source: String,
}

impl Token {
    fn symbol(kind: Kind, source: &str) -> Self {
        Token {
            kind,
            field: String::new(),
            op: String::new(),
            value: String::new(),
            quoted: false,
            source: source.to_owned(),
        }
    }
}

fn is_space(c: char) -> bool {
    c.is_whitespace()
}

fn is_break(c: char) -> bool {
    is_space(c) || c == '(' || c == ')'
}

fn is_operator_char(c: char) -> bool {
    c == '<' || c == '>' || c == '='
}

/// Approximates QChar::isLetter() on UTF-16 code units: characters outside the
/// Basic Multilingual Plane are surrogate pairs there and never letters.
/// Rust's Alphabetic property also includes some marks and letter numbers that
/// Qt does not treat as letters.
fn is_letter(c: char) -> bool {
    (c as u32) <= 0xFFFF && c.is_alphabetic()
}

fn text(chars: &[char]) -> String {
    chars.iter().collect()
}

fn tokenize(input: &str) -> Result<Vec<Token>, String> {
    let t: Vec<char> = input.chars().collect();
    let n = t.len();
    let mut tokens = Vec::new();
    let mut i = 0;
    while i < n {
        let c = t[i];
        if is_space(c) {
            i += 1;
            continue;
        }
        if c == '(' || c == ')' {
            let kind = if c == '(' { Kind::Open } else { Kind::Close };
            tokens.push(Token::symbol(kind, &c.to_string()));
            i += 1;
            continue;
        }
        if c == '-' && i + 1 < n && !is_break(t[i + 1]) {
            tokens.push(Token::symbol(Kind::Not, "-"));
            i += 1;
            continue;
        }
        let start = i;
        let mut token = Token::symbol(Kind::Word, "");
        let mut end = i;
        while end < n && is_letter(t[end]) {
            end += 1;
        }
        if end > i && end < n && (t[end] == ':' || is_operator_char(t[end])) {
            token.field = text(&t[i..end]);
            i = end;
            if t[i] == ':' {
                i += 1;
            }
            let op_start = i;
            while i < n && is_operator_char(t[i]) {
                i += 1;
            }
            token.op = text(&t[op_start..i]);
        }
        if i < n && t[i] == '"' {
            let close = match t[i + 1..].iter().position(|&c| c == '"') {
                Some(offset) => i + 1 + offset,
                None => {
                    return Err(format!(
                        "A quotation mark is not closed: {}",
                        text(&t[start..])
                    ))
                }
            };
            token.value = text(&t[i + 1..close]);
            token.quoted = true;
            i = close + 1;
            if i < n && !is_break(t[i]) {
                return Err(format!(
                    "Separate a quoted value from the following text with a space: {}",
                    text(&t[start..=i])
                ));
            }
        } else {
            let value_start = i;
            while i < n && !is_break(t[i]) {
                if t[i] == '"' {
                    return Err(format!(
                        "Quote a whole value, not part of a word: {}",
                        text(&t[start..=i])
                    ));
                }
                i += 1;
            }
            token.value = text(&t[value_start..i]);
        }
        token.source = text(&t[start..i]);
        if token.field.is_empty() && !token.quoted {
            token.kind = match token.value.as_str() {
                "AND" => Kind::And,
                "OR" => Kind::Or,
                "NOT" => Kind::Not,
                _ => Kind::Word,
            };
        }
        tokens.push(token);
    }
    Ok(tokens)
}

fn field_named(name: &str) -> Option<Field> {
    match name.to_lowercase().as_str() {
        "name" | "filename" => Some(Field::Name),
        "path" => Some(Field::Path),
        "type" => Some(Field::Type),
        "size" => Some(Field::Size),
        "modified" | "date" => Some(Field::Modified),
        "content" | "contents" => Some(Field::Content),
        "tag" | "tags" => Some(Field::Tag),
        "tagged" => Some(Field::Tagged),
        "meta" | "metadata" => Some(Field::Metadata),
        _ => None,
    }
}

fn comparison_for(op: &str) -> Option<Comparison> {
    match op {
        "" => Some(Comparison::Contains),
        "=" => Some(Comparison::Equal),
        "<" => Some(Comparison::Less),
        "<=" => Some(Comparison::LessOrEqual),
        ">" => Some(Comparison::Greater),
        ">=" => Some(Comparison::GreaterOrEqual),
        _ => None,
    }
}

fn ordered(comparison: Comparison) -> bool {
    matches!(
        comparison,
        Comparison::Less
            | Comparison::LessOrEqual
            | Comparison::Greater
            | Comparison::GreaterOrEqual
    )
}

/// PCRE's `\s` without Unicode properties.
fn is_regex_space(c: char) -> bool {
    matches!(c, ' ' | '\t' | '\n' | '\x0B' | '\x0C' | '\r')
}

/// Mirrors `^(\d+(?:\.\d+)?)\s*([kmgt]i?b?|b)?$`, case-insensitive.
fn parse_size(value: &str) -> Option<i64> {
    // PCRE's `$` also matches before a final newline.
    let value = value.strip_suffix('\n').unwrap_or(value);
    let chars: Vec<char> = value.chars().collect();
    let n = chars.len();
    let mut i = 0;
    while i < n && chars[i].is_ascii_digit() {
        i += 1;
    }
    if i == 0 {
        return None;
    }
    if i + 1 < n && chars[i] == '.' && chars[i + 1].is_ascii_digit() {
        i += 1;
        while i < n && chars[i].is_ascii_digit() {
            i += 1;
        }
    }
    let mut number: f64 = text(&chars[..i]).parse().ok()?;
    while i < n && is_regex_space(chars[i]) {
        i += 1;
    }
    let unit = text(&chars[i..]).to_lowercase();
    let exponent = match unit.as_str() {
        "" | "b" => 0,
        _ => {
            let mut letters = unit.chars();
            let first = letters.next()?;
            let rest: String = letters.collect();
            if !matches!(rest.as_str(), "" | "i" | "b" | "ib") {
                return None;
            }
            match first {
                'k' => 1,
                'm' => 2,
                'g' => 3,
                't' => 4,
                _ => return None,
            }
        }
    };
    number *= 1024f64.powi(exponent);
    if number > 9.0e18 {
        return None;
    }
    Some(number.round() as i64)
}

fn days_in_month(year: u32, month: u32) -> u32 {
    match month {
        1 | 3 | 5 | 7 | 8 | 10 | 12 => 31,
        4 | 6 | 9 | 11 => 30,
        _ if (year.is_multiple_of(4) && !year.is_multiple_of(100)) || year.is_multiple_of(400) => {
            29
        }
        _ => 28,
    }
}

fn digits(value: &str, pattern: &str) -> bool {
    value.len() == pattern.len()
        && value.chars().zip(pattern.chars()).all(|(c, p)| {
            if p == 'd' {
                c.is_ascii_digit()
            } else {
                c == p
            }
        })
}

/// Validates the periods QDate accepts; QDate has no year 0.
fn valid_period(value: &str) -> bool {
    let lower = value.to_lowercase();
    if lower == "today" || lower == "yesterday" {
        return true;
    }
    let number = |range: std::ops::Range<usize>| value[range].parse::<u32>().unwrap_or(0);
    let (year, month, day) = if digits(value, "dddd-dd-dd") {
        (number(0..4), number(5..7), number(8..10))
    } else if digits(value, "dddd-dd") {
        (number(0..4), number(5..7), 1)
    } else if digits(value, "dddd") {
        (number(0..4), 1, 1)
    } else {
        return false;
    };
    year != 0 && (1..=12).contains(&month) && day >= 1 && day <= days_in_month(year, month)
}

/// Checks MIME type syntax (RFC 6838 restricted names) in place of a MIME database lookup.
fn valid_mime_syntax(value: &str) -> bool {
    let part = |part: &str| {
        part.chars()
            .next()
            .is_some_and(|c| c.is_ascii_alphanumeric())
            && part
                .chars()
                .all(|c| c.is_ascii_alphanumeric() || "!#$&^_.+-".contains(c))
    };
    match value.split_once('/') {
        Some((kind, subtype)) => part(kind) && part(subtype),
        None => false,
    }
}

fn build_term(token: &Token) -> Result<Term, String> {
    let mut term = Term {
        field: Field::Name,
        comparison: Comparison::Contains,
        text: token.value.clone(),
        type_match: TypeMatch::File,
        size: 0,
    };
    if !token.field.is_empty() {
        term.field = field_named(&token.field).ok_or_else(|| {
            format!(
                "Unknown field “{}”. Use name, path, type, size, modified, content, tag, tagged or meta, or quote the text to search for it literally.",
                token.field
            )
        })?;
    }
    term.comparison = comparison_for(&token.op).ok_or_else(|| {
        format!(
            "Unknown comparison “{}” in {}. Use :, =, <, <=, > or >=.",
            token.op, token.source
        )
    })?;
    if term.text.is_empty() {
        return Err(if token.field.is_empty() {
            "Enter text inside the quotation marks.".to_owned()
        } else {
            format!("Enter a value after {}.", token.source)
        });
    }
    let unknown_type = |text: &str| {
        format!(
            "Unknown file type “{text}”. Use file, folder, an extension such as pdf, or a MIME type such as image/png or image/*."
        )
    };
    match term.field {
        Field::Name | Field::Path | Field::Content | Field::Tag | Field::Metadata => {
            if ordered(term.comparison) {
                return Err(if term.field == Field::Metadata {
                    format!(
                        "Metadata criteria can't use <, <=, > or >=: {}",
                        token.source
                    )
                } else if term.field == Field::Tag {
                    format!("Tag criteria can't use <, <=, > or >=: {}", token.source)
                } else {
                    format!(
                        "Name, path and content criteria can't use <, <=, > or >=: {}",
                        token.source
                    )
                });
            }
            if term.field == Field::Content && term.comparison == Comparison::Equal {
                return Err(format!(
                    "Content criteria match text within files; use content: instead of content=: {}",
                    token.source
                ));
            }
        }
        Field::Type => {
            if ordered(term.comparison) {
                return Err(format!(
                    "Type criteria can't use <, <=, > or >=: {}",
                    token.source
                ));
            }
            let value = term.text.to_lowercase();
            if value == "file" || value == "files" {
                term.type_match = TypeMatch::File;
            } else if matches!(value.as_str(), "folder" | "folders" | "dir" | "directory") {
                term.type_match = TypeMatch::Folder;
            } else if value.ends_with("/*")
                && value.matches('/').count() == 1
                && value.chars().count() > 2
            {
                term.type_match = TypeMatch::MimePrefix;
                term.text = value[..value.len() - 1].to_owned();
            } else if value.contains('/') {
                if !valid_mime_syntax(&value) {
                    return Err(unknown_type(&term.text));
                }
                term.type_match = TypeMatch::MimeType;
                term.text = value;
            } else {
                let extension = value
                    .strip_prefix("*.")
                    .or_else(|| value.strip_prefix('.'))
                    .unwrap_or(&value);
                if extension.is_empty() || extension.contains('*') || extension.contains('?') {
                    return Err(unknown_type(&term.text));
                }
                term.type_match = TypeMatch::Extension;
                term.text = extension.to_owned();
            }
        }
        Field::Size => {
            term.size = parse_size(&term.text).ok_or_else(|| {
                format!(
                    "“{}” is not a valid size. Use a number with an optional unit, such as 500K, 10M or 2G.",
                    term.text
                )
            })?;
            if term.comparison == Comparison::Contains {
                term.comparison = Comparison::Equal;
            }
        }
        Field::Modified => {
            if !valid_period(&term.text) {
                return Err(format!(
                    "“{}” is not a valid date. Use YYYY-MM-DD, YYYY-MM, YYYY, today or yesterday.",
                    term.text
                ));
            }
            if term.comparison == Comparison::Contains {
                term.comparison = Comparison::Equal;
            }
        }
        Field::Tagged => {
            if ordered(term.comparison) {
                return Err(format!(
                    "Tag criteria can't use <, <=, > or >=: {}",
                    token.source
                ));
            }
            term.text = match term.text.to_lowercase().as_str() {
                "yes" | "true" => "yes".to_owned(),
                "no" | "false" => "no".to_owned(),
                _ => {
                    return Err(format!(
                        "“{}” is not a valid tagged value. Use tagged:yes or tagged:no.",
                        term.text
                    ));
                }
            };
            term.comparison = Comparison::Contains;
        }
    }
    Ok(term)
}

/// Joins two nodes with AND or OR, flattening nested groups of the same kind.
fn combine(left: Node, right: Node, and: bool) -> Node {
    let mut children = match left {
        Node::And(children) if and => children,
        Node::Or(children) if !and => children,
        other => vec![other],
    };
    match right {
        Node::And(more) if and => children.extend(more),
        Node::Or(more) if !and => children.extend(more),
        other => children.push(other),
    }
    if and {
        Node::And(children)
    } else {
        Node::Or(children)
    }
}

struct Parser {
    tokens: Vec<Token>,
    position: usize,
}

impl Parser {
    fn peek(&self) -> Option<&Token> {
        self.tokens.get(self.position)
    }

    fn peek_kind(&self) -> Option<Kind> {
        self.peek().map(|token| token.kind)
    }

    fn parse(&mut self) -> Result<Node, String> {
        let node = self.parse_or()?;
        if self.position < self.tokens.len() {
            return Err("A closing parenthesis has no matching opening parenthesis.".to_owned());
        }
        Ok(node)
    }

    fn parse_or(&mut self) -> Result<Node, String> {
        let mut node = self.parse_and("")?;
        while self.peek_kind() == Some(Kind::Or) {
            self.position += 1;
            let right = self.parse_and("OR")?;
            node = combine(node, right, false);
        }
        Ok(node)
    }

    fn parse_and(&mut self, after: &str) -> Result<Node, String> {
        let mut node = self.parse_unary(after)?;
        while let Some(kind) = self.peek_kind() {
            if kind == Kind::Or || kind == Kind::Close {
                break;
            }
            let mut operator_name = "";
            if kind == Kind::And {
                self.position += 1;
                operator_name = "AND";
            }
            let right = self.parse_unary(operator_name)?;
            node = combine(node, right, true);
        }
        Ok(node)
    }

    fn parse_unary(&mut self, after: &str) -> Result<Node, String> {
        if let Some(token) = self.peek().filter(|token| token.kind == Kind::Not) {
            let source = token.source.clone();
            self.position += 1;
            return Ok(match self.parse_unary(&source)? {
                Node::Not(inner) => *inner,
                child => Node::Not(Box::new(child)),
            });
        }
        self.parse_primary(after)
    }

    fn parse_primary(&mut self, after: &str) -> Result<Node, String> {
        let add_term = || format!("Add a search term after {after}.");
        let Some(token) = self.peek().cloned() else {
            return Err(if after.is_empty() {
                "Enter text to search for.".to_owned()
            } else {
                add_term()
            });
        };
        match token.kind {
            Kind::Open => {
                self.position += 1;
                if self.peek_kind() == Some(Kind::Close) {
                    return Err("Parentheses must contain a search term.".to_owned());
                }
                let node = self.parse_or()?;
                if self.peek_kind() != Some(Kind::Close) {
                    return Err("An opening parenthesis is not closed.".to_owned());
                }
                self.position += 1;
                Ok(node)
            }
            Kind::Close => Err(if after.is_empty() {
                "A closing parenthesis has no matching opening parenthesis.".to_owned()
            } else {
                add_term()
            }),
            Kind::And | Kind::Or => Err(format!(
                "{} must be placed between two search terms. Quote it to search for the word.",
                token.source
            )),
            Kind::Not | Kind::Word => {
                self.position += 1;
                build_term(&token).map(Node::Term)
            }
        }
    }
}

/// Parses a query, returning its tree or an English explanation of the first error.
pub fn parse(input: &str) -> Result<Node, String> {
    Parser {
        tokens: tokenize(input)?,
        position: 0,
    }
    .parse()
}

impl fmt::Display for Node {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Node::Term(term) => {
                let field = match term.field {
                    Field::Name => "name",
                    Field::Path => "path",
                    Field::Type => "type",
                    Field::Size => "size",
                    Field::Modified => "modified",
                    Field::Content => "content",
                    Field::Tag => "tag",
                    Field::Tagged => "tagged",
                    Field::Metadata => "meta",
                };
                let op = match term.comparison {
                    Comparison::Contains => ":",
                    Comparison::Equal => "=",
                    Comparison::Less => "<",
                    Comparison::LessOrEqual => "<=",
                    Comparison::Greater => ">",
                    Comparison::GreaterOrEqual => ">=",
                };
                let star = if term.field == Field::Type && term.type_match == TypeMatch::MimePrefix
                {
                    "*"
                } else {
                    ""
                };
                write!(f, "{field}{op}\"{}{star}\"", term.text)
            }
            Node::Not(child) => write!(f, "NOT {child}"),
            Node::And(children) | Node::Or(children) => {
                let separator = if matches!(self, Node::And(_)) {
                    " AND "
                } else {
                    " OR "
                };
                f.write_str("(")?;
                for (index, child) in children.iter().enumerate() {
                    if index > 0 {
                        f.write_str(separator)?;
                    }
                    write!(f, "{child}")?;
                }
                f.write_str(")")
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn normalized(input: &str) -> String {
        parse(input)
            .map(|node| node.to_string())
            .unwrap_or_else(|error| format!("error: {error}"))
    }

    #[test]
    fn normalizes_grouping() {
        assert_eq!(
            normalized("a b OR c"),
            "((name:\"a\" AND name:\"b\") OR name:\"c\")"
        );
        assert_eq!(
            normalized("(a OR b) OR c"),
            "(name:\"a\" OR name:\"b\" OR name:\"c\")"
        );
        assert_eq!(normalized("NOT -a"), "name:\"a\"");
        assert_eq!(normalized("NOT (a b)"), "NOT (name:\"a\" AND name:\"b\")");
        // As in C++, a minus sign before a parenthesis is a literal word.
        assert_eq!(
            normalized("-(a b)"),
            "(name:\"-\" AND name:\"a\" AND name:\"b\")"
        );
        assert_eq!(normalized("type:FOLDER"), "type:\"FOLDER\"");
        assert_eq!(normalized("type:Image/*"), "type:\"image/*\"");
        assert_eq!(normalized("size:10M"), "size=\"10M\"");
        assert_eq!(
            normalized("Tags=Cat tagged:FALSE"),
            "(tag=\"Cat\" AND tagged:\"no\")"
        );
        assert_eq!(
            normalized("Metadata:Canon meta=Jazz"),
            "(meta:\"Canon\" AND meta=\"Jazz\")"
        );
    }

    #[test]
    fn rejects_invalid_queries() {
        for input in [
            "",
            "a OR",
            "(a",
            "a)",
            "foo:bar",
            "\"a",
            "size:big",
            "modified:2025-02-29",
            "content=x",
            "tag>a",
            "tagged:maybe",
            "tagged<yes",
            "meta>2020",
        ] {
            assert!(parse(input).is_err(), "{input:?} should be invalid");
        }
        assert!(parse("modified:2024-02-29").is_ok());
        assert!(parse("modified:0000").is_err());
    }

    #[test]
    fn parses_sizes() {
        assert_eq!(parse_size("1.5K"), Some(1536));
        assert_eq!(parse_size("2 KiB"), Some(2048));
        assert_eq!(parse_size("1g"), Some(1 << 30));
        assert_eq!(parse_size("10\n"), Some(10));
        assert_eq!(parse_size("10."), None);
        assert_eq!(parse_size("9000000T"), None);
    }
}
