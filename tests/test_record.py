import miniosl


def test_record():
    r = miniosl.usi_record('startpos moves 2g2f 8c8d 2f2e 8d8e 9g9f 4a3b 3i3h 7a7b 6i7h 5a5b 4g4f 9c9d 3h4g 7c7d 1g1f 7d7e 2e2d 2c2d 2h2d 8e8f 8g8f P*2c 2d7d 8b8f 5i5h 7b7c 7d7e 2c2d P*8g 8f8b 7e3e 3b2c 3e3f 2d2e 7g7f 5b4b 1f1e 3c3d 4i3h 7c6d 8h2b+ 3a2b 4f4e 2b3c 4g5f 8a7c 6g6f P*8f 8g8f 8b8f P*8g 8f8a 3f4f 6a6b 7i6h 5c5d 6h6g 4b3b 3g3f 5d5e 5f4g 6d5c 2i3g 6c6d 7f7e 8a8d 5h4h B*2d 3f3e 2d3e 4f3f P*8h 7h8h 1c1d 1e1d 1a1d 1i1d 2c1d P*2b 3c2b 4e4d 4c4d 8h7h 6d6e P*1b 1d2d L*7d P*7b 1b1a+ 2b1a 8i7g 4d4e 7d7c+ 7b7c 7g6e L*4f 6e5c 4f4g 4h5h 3e1g+ 3g4e 2d3e P*4d 8d4d P*3c 2a3c B*2a 3b2a 4e3c+ S*3b S*2d 4g4h+ 5h6h 4h5h 6g5h P*6g 7h6g B*1e 2d1e 3b3c 3f3g 1g1f L*1g N*4f 1g1f 4f5h+ 6h5h 6b5c B*6b 4d4c N*2d S*2c P*4d 5c4d B*5b 1a2b 5b4c+ 4d4c R*4a B*3a 4a4c+ 3c2d 1e2d N*4f 5h4g 3e3f 3g3f N*3e 2d3e L*1d S*3b 2a1a G*2a 1a1b 4c2c 2b2c N*2d 1b1c 3b2c+ 1c2c 3e3d 2c2d G*2c')
    assert len(r) > 10
    assert r.to_usi() != ''
    s = r.replay(15)
    assert isinstance(s, miniosl.State)
    anim = r.to_apng(10)
    assert anim
