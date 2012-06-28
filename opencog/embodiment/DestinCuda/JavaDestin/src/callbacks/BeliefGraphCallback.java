package callbacks;

import javadestin.*;


public class BeliefGraphCallback extends DestinIterationFinishedCallback {
	private FPSCallback fps = new FPSCallback();
	
@Override
	public void callback(RunningInfo info, INetwork network) {
                
		int states = network.getBeliefsPerNode(7);
		float [] beliefs = Util.toJavaArray(network.getNodeBeliefs(7, 0, 0), states);
		
		int beliefWidth = 20;
		System.out.print("\033[2J");//clearscreen
		System.out.flush();
		
		fps.callback(info, network);
		
		for(int s = 0 ; s < states; s++){
			int b = (int)(beliefs[s] * beliefWidth);
			for(int i = 0 ; i < b ; i++){
				System.out.print(s);
			}
			System.out.println();
		}
		System.out.println("-------------------");
		
	}	

}
