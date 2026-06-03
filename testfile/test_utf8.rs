// test_utf8.rs - UTF-8 test for mel editor
// Art2Dec SoftLab - mshell Ecosystem
fn greet(name: &str) -> String {
    format!("Hello, {}! Welcome.", name)
}
fn count_chars(s: &str) -> usize {
    s.chars().count()
}
fn main() {
    // English input strings
    let msg = "UTF-8 testing in the mel editor";
    let name = "Igor";
    println!("{}", greet(name));
    println!("String: {}", msg);
    println!("Bytes: {}, characters: {}", msg.len(), count_chars(msg));
    // Different languages
    let langs: Vec<&str> = vec![
        "Russian: привет мир",
        "Deutsch: Hallo Welt",
        "Japanese: こんにちは",
        "Chinese: 你好世界",
    ];
    for lang in &langs {
        println!("{}", lang);
    }
    // Character iteration
    let word = "mshell";
    let upper: String = word
        .chars()
        .map(|c| c.to_uppercase().next().unwrap())
        .collect();
    println!("Upper: {}", upper);
}
