use std::{
    fs::File,
    io::{Read, Write},
    path::PathBuf,
    str::FromStr,
};

use anyhow::Result;
use base64::{engine::general_purpose, Engine};
use clap::Parser;
use sev::{
    firmware::guest::Firmware,
    launch::snp::{Policy, PolicyFlags},
    measurement::{
        idblock::{generate_key_digest, load_priv_key, snp_calculate_id},
        idblock_types::{FamilyId, IdAuth, IdBlock, IdBlockLaunchDigest, IdMeasurements, ImageId},
        large_array::LargeArray,
        snp::snp_calc_launch_digest,
    },
    Version,
};

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    ///Binary file containing the expected digest
    ///No clue how to calculate this. For now, start the VM and use the value from the
    ///attestation report
    #[arg(short, long)]
    launch_digest_path: String,
    /// ID_KEY from Table 75 in SNP Docs. Used to sign the identity block
    /// PEM encoded ECDSA P-384 private key.
    #[arg(short, long)]
    id_key_path: String,
    /// AUTHOR_KEY from Table 75 in SNP Docs
    /// PEM encoded ECDSA P-384 private key.
    #[arg(short, long)]
    auth_key_path: String,
}

fn gen_and_print_id_block(args: &Args) -> Result<IdMeasurements> {
    let mut ld_file = File::open(&args.launch_digest_path)?;
    let mut ld_buf = Vec::new();
    ld_file.read_to_end(&mut ld_buf)?;
    println!("Launch Digest file has {} bytes", ld_buf.len());

    //TODO: there is a `snp_calc_launch_digest()` function. Lookinto that later

    //based on the unit test in https://github.com/virtee/sev/blob/main/tests/id-block.rs

    let ld = IdBlockLaunchDigest::new(LargeArray::try_from(ld_buf)?);
    let f_id = FamilyId::default();
    let i_id = ImageId::default();
    let policy = Policy {
        flags: PolicyFlags::SMT,
        minfw: Version { major: 0, minor: 0 },
    };
    // let id_block = IdBlock::new(, , , , )
    let block_calculations = snp_calculate_id(
        Some(ld),
        Some(f_id),
        Some(i_id),
        None, //SVN is the "Security Version Number" of the PSP
        Some(policy.into()),
        args.id_key_path.clone().into(),
        args.auth_key_path.clone().into(),
    )?;

    Ok(block_calculations)
}

fn main() -> Result<()> {
    let args = Args::parse();

    let block_calculations = gen_and_print_id_block(&args)?;

    let id_block_string =
        general_purpose::STANDARD.encode(bincode::serialize(&block_calculations.id_block).unwrap());
    let id_key_digest_string = general_purpose::STANDARD
        .encode::<Vec<u8>>(block_calculations.id_key_digest.try_into().unwrap());
    let auth_key_digest_string = general_purpose::STANDARD
        .encode::<Vec<u8>>(block_calculations.auth_key_digest.try_into().unwrap());
    //dont print as it is quite long
    let auth_block_string =
        general_purpose::STANDARD.encode(bincode::serialize(&block_calculations.id_auth)?);

    println!("id block: {}", id_block_string);
    println!("id key digest: {}", id_key_digest_string);
    println!("auth key digest: {}", auth_key_digest_string);
    println!("writing id auth data bae64 encoded to ./auth-block.txt");
    let mut auth_block_file = File::create("./auth-block.txt")?;
    auth_block_file.write_all(auth_block_string.as_bytes())?;
    println!("writing id block to ./id-block.txt");
    let mut id_block_file = File::create("./id-block.txt")?;
    id_block_file.write_all(id_block_string.as_bytes())?;
    Ok(())
}
