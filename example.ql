type Option a = { 
    None, 
    Some a 
}


fun foo opt = {
    match opt with {
        Some x -> { x }
        None -> { 0 }
    }
}

fun main = { foo (Some 2) }
