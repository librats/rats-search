export default (Superclass) => {
	let instance;
    
	return class Singleton extends Superclass {
		constructor(props) {
			super(props)
			if (instance) {
				instance.props = props
				return instance;
			}
			instance = this;
		}
		static instance() {
			if (instance) {
				return instance;
			} else {
				return new Singleton();             
			}
		}
		static do(key, ...params) {
			if ( typeof this.instance()[key] === 'function') {
				return this.instance()[key](...params);         
			} else {
				return this.instance()[key];
			}
		}
	}
}

