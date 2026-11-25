// SPDX-License-Identifier: GPL-3.0-or-later

//! Prototype of the Plasma Opus advanced search query parser in Rust.
//!
//! The parser mirrors `src/core/searchquery.cpp`. The C ABI below lets the
//! C++ side validate and normalize a query; evaluation stays in C++.

pub mod parser;

use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;

/// The query is valid; the output holds its normalized form.
pub const OPUS_QUERY_VALID: i32 = 0;
/// The query is invalid; the output holds an English explanation.
pub const OPUS_QUERY_INVALID: i32 = 1;
/// The input isn't valid UTF-8; there is no output.
pub const OPUS_QUERY_BAD_ENCODING: i32 = 2;
/// A required pointer is null; there is no output.
pub const OPUS_QUERY_BAD_ARGUMENT: i32 = 3;
/// The parser panicked; there is no output.
pub const OPUS_QUERY_INTERNAL_ERROR: i32 = 4;

fn check(text: &[u8]) -> (i32, Option<String>) {
    let Ok(text) = std::str::from_utf8(text) else {
        return (OPUS_QUERY_BAD_ENCODING, None);
    };
    match parser::parse(text) {
        Ok(node) => (OPUS_QUERY_VALID, Some(node.to_string())),
        Err(error) => (OPUS_QUERY_INVALID, Some(error)),
    }
}

/// Runs @p f, turning a panic into `OPUS_QUERY_INTERNAL_ERROR` so that it never unwinds into C++.
fn run_guarded<F>(f: F) -> (i32, Option<String>)
where
    F: FnOnce() -> (i32, Option<String>),
{
    catch_unwind(AssertUnwindSafe(f)).unwrap_or((OPUS_QUERY_INTERNAL_ERROR, None))
}

/// Parses `len` bytes of UTF-8 at `text`.
///
/// On `OPUS_QUERY_VALID` and `OPUS_QUERY_INVALID`, stores a UTF-8 buffer that
/// isn't NUL-terminated in `*out` and `*out_len`; release it with
/// `opus_query_free`. Otherwise stores null and 0. `text` may be null only
/// when `len` is 0.
///
/// # Safety
///
/// `text` must point to `len` readable bytes unless `len` is 0; `out` and
/// `out_len` must be valid for writes.
#[no_mangle]
pub unsafe extern "C" fn opus_query_check(
    text: *const u8,
    len: usize,
    out: *mut *mut u8,
    out_len: *mut usize,
) -> i32 {
    if out.is_null() || out_len.is_null() {
        return OPUS_QUERY_BAD_ARGUMENT;
    }
    *out = ptr::null_mut();
    *out_len = 0;
    if text.is_null() && len != 0 {
        return OPUS_QUERY_BAD_ARGUMENT;
    }
    let input: &[u8] = if len == 0 {
        &[]
    } else {
        std::slice::from_raw_parts(text, len)
    };
    let (status, message) = run_guarded(|| check(input));
    if let Some(message) = message {
        let bytes: Box<[u8]> = message.into_bytes().into_boxed_slice();
        *out_len = bytes.len();
        *out = Box::into_raw(bytes) as *mut u8;
    }
    status
}

/// Releases a buffer returned by `opus_query_check`. Null is ignored.
///
/// # Safety
///
/// `ptr` and `len` must come from one `opus_query_check` call and must not be freed twice.
#[no_mangle]
pub unsafe extern "C" fn opus_query_free(ptr: *mut u8, len: usize) {
    if !ptr.is_null() {
        drop(Box::from_raw(ptr::slice_from_raw_parts_mut(ptr, len)));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn call(text: &[u8]) -> (i32, Option<String>) {
        let mut out = ptr::null_mut();
        let mut out_len = 0;
        let status = unsafe { opus_query_check(text.as_ptr(), text.len(), &mut out, &mut out_len) };
        let message = (!out.is_null()).then(|| {
            let bytes = unsafe { std::slice::from_raw_parts(out, out_len) }.to_vec();
            unsafe { opus_query_free(out, out_len) };
            String::from_utf8(bytes).unwrap()
        });
        (status, message)
    }

    #[test]
    fn returns_normalized_queries_and_errors() {
        assert_eq!(
            call(b"a OR b"),
            (OPUS_QUERY_VALID, Some("(name:\"a\" OR name:\"b\")".into()))
        );
        let (status, message) = call(b"a OR");
        assert_eq!(status, OPUS_QUERY_INVALID);
        assert!(!message.unwrap().is_empty());
    }

    #[test]
    fn rejects_invalid_utf8() {
        assert_eq!(call(b"name:\xff"), (OPUS_QUERY_BAD_ENCODING, None));
    }

    #[test]
    fn rejects_null_arguments() {
        let mut out = ptr::null_mut();
        let mut out_len = 0;
        unsafe {
            assert_eq!(
                opus_query_check(ptr::null(), 1, &mut out, &mut out_len),
                OPUS_QUERY_BAD_ARGUMENT
            );
            assert_eq!(
                opus_query_check(b"a".as_ptr(), 1, ptr::null_mut(), &mut out_len),
                OPUS_QUERY_BAD_ARGUMENT
            );
            assert_eq!(
                opus_query_check(b"a".as_ptr(), 1, &mut out, ptr::null_mut()),
                OPUS_QUERY_BAD_ARGUMENT
            );
            // An empty query may be null; it's invalid rather than a bad argument.
            assert_eq!(
                opus_query_check(ptr::null(), 0, &mut out, &mut out_len),
                OPUS_QUERY_INVALID
            );
            opus_query_free(out, out_len);
            opus_query_free(ptr::null_mut(), 0);
        }
    }

    #[test]
    fn contains_panics() {
        let previous = std::panic::take_hook();
        std::panic::set_hook(Box::new(|_| {}));
        let result = run_guarded(|| panic!("boom"));
        std::panic::set_hook(previous);
        assert_eq!(result, (OPUS_QUERY_INTERNAL_ERROR, None));
    }
}
