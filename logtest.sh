echo "-----------parque.log-----------"
echo "numero de estacionamentos"
grep -c estacionamento parque.log
echo "numero de saidas"
grep -c saida parque.log
echo "numero de carros que nao conseguiram entrar pelo parque estar cheio"
grep -c cheio parque.log

echo "-----------gerador.log-----------"
echo "numero de entradas"
grep -c entrada gerador.log
echo "numero de saidas"
grep -c saida gerador.log
echo "numero de carros que nao conseguiram entrar pelo parque estar cheio"
grep -c cheio! gerador.log
echo "numero de carros que nao conseguiram entrar pelo parque estar encerrado"
grep -c encerrado gerador.log
