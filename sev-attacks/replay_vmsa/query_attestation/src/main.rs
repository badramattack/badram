use sev::firmware::guest::{DerivedKey, Firmware, GuestFieldSelect};

fn main() {
    println!("Hello, world!");

    let unique_data: [u8; 64] = [42; 64];
    let mut fw = Firmware::open().expect("fw open failed");
    let report = fw
        .get_report(None, Some(unique_data), None)
        .expect("failed to get report");
    println!("Measurement digest: {:x?}", report.measurement);

    //See "Table 18 MSG_KEY_REQ Message Structure" in SEV-SNP ABI doc for parameter explanation
    let key_derive_req = DerivedKey::new(false, GuestFieldSelect(0x1f), 0, 0, 0);
    let mut fw = Firmware::open().expect("Failed to open FW instance");
    let derived_key = fw
        .get_derived_key(None, key_derive_req)
        .expect("Failed to query derived key");

    println!("Sealing Key: {:X?}", derived_key);
}
