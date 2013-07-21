BEGIN{

}{
  packet_size = $1;
  aloha_num = $2;
  aloha_energy = $3;
  bcst_num = $4;
  bcst_energy = $5;
  printf("%d %f %f\n",packet_size,
      (aloha_energy/(packet_size*aloha_num)),
      bcst_energy/(packet_size*bcst_num));
}
END{

}